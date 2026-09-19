#include "GtsModelMaterialRealization.h"

#include <stdexcept>
#include "MaterialAssetRealization.h"
#include "assets/loading/model/GtsModelResource.h"
#include "assets/model/GtsModelAsset.h"
#include "assets/processing/image/GtsScalarImagePacking.h"

using gts::rendering::MaterialAssetLoader;
using gts::rendering::MaterialAssetRealization;
using gts::rendering::MaterialRuntime;

class RuntimeModelMaterials final : public GtsRealizedModelMaterials
{
    public:
    // Associations are scoped by the exact realized model, never by a naked slot from another model.
    MaterialInstanceHandle materialFor(const GtsRealizedModel& owner, uint32_t geometry, uint32_t primitive) const override;
    bool                   valid() const override;
    bool isMaterialAlive(MaterialInstanceHandle material) const override
    {
        return !lifetime.expired() && runtime->isInstanceAlive(material);
    }
    bool belongsTo(const GtsRealizedModel& owner) const override { return model.get() == &owner && valid(); }

    std::weak_ptr<const int> scopeToken() const override { return lifetime; }

    private:
    friend class GtsModelMaterialRealization;
    std::shared_ptr<const GtsRealizedModel>          model;
    gts::rendering::MaterialRuntime*                 runtime = nullptr;
    std::weak_ptr<const int>                         lifetime;
    std::vector<std::vector<MaterialInstanceHandle>> bindings;
};

bool RuntimeModelMaterials::valid() const
{
    if (lifetime.expired())
        return false;
    for (const auto& geometry : bindings)
        for (auto handle : geometry)
            if (!runtime->isInstanceAlive(handle))
                return false;
    return true;
}

MaterialInstanceHandle
RuntimeModelMaterials::materialFor(const GtsRealizedModel& owner, uint32_t geometry, uint32_t primitive) const
{
    if (&owner != model.get() || lifetime.expired() || geometry >= bindings.size() ||
        primitive >= bindings[geometry].size())
        return {};
    const auto handle = bindings[geometry][primitive];
    return runtime->isInstanceAlive(handle) ? handle : MaterialInstanceHandle{};
}

GtsModelMaterialRealization::GtsModelMaterialRealization(MaterialRuntime& runtime, IResourceProvider* resources)
    : runtime(runtime), resources(resources), lifetime(runtime.lifetimeToken())
{
}

bool GtsModelMaterialRealization::belongsTo(const MaterialRuntime& owner, const IResourceProvider* provider) const
{
    return !lifetime.expired() && lifetime.lock() == owner.lifetimeToken().lock() && resources == provider;
}

std::shared_ptr<const GtsDecodedImage> GtsModelMaterialRealization::image(const GtsModelHandle& model, uint32_t index)
{
    auto& cache = images[model];
    if (auto found = cache.find(index); found != cache.end())
        return found->second;
    const auto* canonical = model->canonicalModel();
    if (!canonical || index >= canonical->images.size())
        throw std::runtime_error("Invalid image index " + std::to_string(index));
    const auto& source  = canonical->images[index].source;
    auto        decoded = std::make_shared<GtsDecodedImage>();
    std::string error;
    if (const auto* path = std::get_if<std::filesystem::path>(&source))
    {
        if (!path->is_absolute())
            throw std::runtime_error("Canonical image lacks an absolute imported source path: " + path->string());
        const auto normalized = std::filesystem::weakly_canonical(*path);
        if (auto found = externalImages.find(normalized); found != externalImages.end())
            return cache[index] = found->second;
        if (!decodeGtsImage(normalized, *decoded, &error))
            throw std::runtime_error(error);
        externalImages[normalized] = decoded;
    }
    else if (!decodeGtsImage(std::span<const uint8_t>(std::get<GtsModelEmbeddedImage>(source).bytes), *decoded, &error))
        throw std::runtime_error("Image " + std::to_string(index) + ": " + error);
    return cache[index] = std::move(decoded);
}

MaterialTextureBinding GtsModelMaterialRealization::texture(std::shared_ptr<const GtsDecodedImage> image,
                                                            TextureColorSpace                      space)
{
    const auto key = std::make_pair(image, space);
    if (auto found = textures.find(key); found != textures.end())
        return MaterialTextureBinding::resolved(found->second, space);
    if (!resources)
        throw std::runtime_error("Texture realization requires a resource provider");
    const auto id = resources->requestMemoryTexture(std::move(image), space);
    if (!id)
        throw std::runtime_error("Resource provider failed to create a memory texture");
    textures.emplace(key, id);
    return MaterialTextureBinding::resolved(id, space);
}

MaterialInstance GtsModelMaterialRealization::canonicalInstance(const GtsModelHandle& model, uint32_t slot)
{
    const auto* canonical = model->canonicalModel();
    if (!canonical || slot >= canonical->materials.size())
        throw std::runtime_error("Invalid canonical material slot");
    const auto&      source = canonical->materials[slot];
    MaterialInstance instance;
    instance.baseColor                = source.baseColor;
    instance.metallic                 = source.metallic;
    instance.roughness                = source.roughness;
    instance.normalScale              = source.normalScale;
    instance.ambientOcclusionStrength = source.ambientOcclusionStrength;
    instance.emissiveFactor           = source.emissiveFactor;
    instance.emissiveStrength         = source.emissiveStrength;
    instance.renderState.alphaMode    = source.alphaMode == GtsModelAlphaMode::Blend  ? MaterialAlphaMode::Blend
                                        : source.alphaMode == GtsModelAlphaMode::Mask ? MaterialAlphaMode::Mask
                                                                                      : MaterialAlphaMode::Opaque;
    instance.renderState.alphaCutoff  = source.alphaCutoff;
    instance.renderState.doubleSided  = source.doubleSided;
    instance.renderState.depthWrite   = source.alphaMode != GtsModelAlphaMode::Blend;
    auto check                        = [](const GtsModelImageBinding& binding)
    {
        if (binding.texCoordSet != 0)
            throw std::runtime_error("Runtime material profile supports only UV0; requested UV" +
                                     std::to_string(binding.texCoordSet));
    };
    auto direct = [&](const std::optional<GtsModelImageBinding>& binding, TextureColorSpace space)
    {
        if (!binding)
            return MaterialTextureBinding::assetPath({}, space);
        check(*binding);
        return texture(image(model, binding->imageIndex), space);
    };
    instance.baseColorTexture = direct(source.baseColorImage, TextureColorSpace::SRgb);
    instance.normalTexture    = direct(source.normalImage, TextureColorSpace::Linear);
    instance.emissiveTexture  = direct(source.emissiveImage, TextureColorSpace::SRgb);
    auto scalar               = [&](const std::optional<GtsModelScalarImageBinding>& first,
                                    const std::optional<GtsModelScalarImageBinding>& second,
                                    bool                                             metallicRoughness)
    {
        if (!first && !second)
            return MaterialTextureBinding::dataAssetPath({});
        auto keyFor = [&](const auto& binding)
        {
            if (!binding)
                return std::string("none");
            check(binding->image);
            return std::to_string(binding->image.imageIndex) + ":" + std::to_string(static_cast<int>(binding->channel));
        };
        const auto key   = std::string(metallicRoughness ? "mr:" : "ao:") + keyFor(first) + "/" + keyFor(second);
        auto&      cache = packedImages[model];
        if (auto found = cache.find(key); found != cache.end())
            return texture(found->second, TextureColorSpace::Linear);
        const auto a = first ? image(model, first->image.imageIndex) : nullptr;
        const auto b = second ? image(model, second->image.imageIndex) : nullptr;
        std::array<std::optional<GtsScalarImageChannel>, 4> channels;
        if (a)
            channels[metallicRoughness ? 2 : 0] =
                GtsScalarImageChannel{a->width, a->height, a->rgba8Pixels, static_cast<uint32_t>(first->channel)};
        if (b)
            channels[1] =
                GtsScalarImageChannel{b->width, b->height, b->rgba8Pixels, static_cast<uint32_t>(second->channel)};
        auto        packed = std::make_shared<GtsDecodedImage>();
        std::string error;
        if (!packGtsScalarImages(channels, *packed, &error))
            throw std::runtime_error(error);
        cache[key] = packed;
        return texture(std::move(packed), TextureColorSpace::Linear);
    };
    instance.metallicRoughnessTexture = scalar(source.metallicImage, source.roughnessImage, true);
    instance.ambientOcclusionTexture  = scalar(source.ambientOcclusionImage, {}, false);
    return instance;
}

GtsModelMaterialResult GtsModelMaterialRealization::realize(std::shared_ptr<const GtsRealizedModel> model)
{
    const std::string identity = model && model->model ? model->model->identityPath().string() : "model";
    std::string       context  = identity;
    std::vector<MaterialInstanceHandle> allocated;
    try
    {
        if (lifetime.expired())
            throw std::runtime_error("Material runtime has expired");
        if (!model || !model->model)
            throw std::runtime_error("Missing realized model");
        if (auto found = models.find(model); found != models.end() && found->second.materials->valid())
            return found->second;
        struct Pending
        {
            MaterialInstanceHandle  handle;
            MaterialInstance        instance;
            MaterialShaderFamily    family = MaterialShaderFamily::StandardSurface;
            std::optional<uint32_t> slot;
        };
        std::map<std::string, Pending>        pending;
        std::vector<std::vector<std::string>> keys;
        for (const auto& geometry : model->geometry)
        {
            auto& ranges = keys.emplace_back();
            for (const auto& primitive : geometry.primitives())
            {
                std::string             key;
                std::optional<uint32_t> slot;
                std::filesystem::path   path;
                if (const auto* canonical = std::get_if<uint32_t>(&primitive.material))
                {
                    slot = *canonical;
                    key  = "slot:" + std::to_string(*slot);
                }
                else if (const auto* external = std::get_if<GtsExternalMaterialReference>(&primitive.material))
                {
                    if (external->reference.logicalPath.empty())
                        throw std::runtime_error("ID-only material reference has no path resolver");
                    path = std::filesystem::weakly_canonical(external->referenceDirectory /
                                                             external->reference.logicalPath);
                    key  = "external:" + std::to_string(external->reference.id) + ":" + path.generic_string();
                }
                ranges.push_back(key);
                if (key.empty() || pending.contains(key))
                    continue;
                context = identity + ": " + key;
                Pending item;
                item.slot   = slot;
                item.handle = slot ? canonicalHandles[model->model][*slot] : externalHandles[key];
                if (!runtime.isInstanceAlive(item.handle))
                {
                    item.handle = {};
                    if (slot)
                        item.instance = canonicalInstance(model->model, *slot);
                    else
                    {
                        gts::rendering::MaterialAssetData data;
                        std::string                       error;
                        if (!MaterialAssetLoader::load(path, data, &error))
                            throw std::runtime_error(error);
                        for (const auto* reference : {&data.baseColorTexture,
                                                      &data.normalTexture,
                                                      &data.emissiveTexture,
                                                      &data.metallicRoughnessTexture,
                                                      &data.ambientOcclusionTexture})
                            if (reference->id != 0 && reference->logicalPath.empty())
                                throw std::runtime_error("ID-only texture reference has no path resolver");
                        item.family   = data.shaderFamily;
                        item.instance = MaterialAssetRealization::makeInstance(data, {}, path.parent_path());
                    }
                    // The existing runtime resolves cooked texture paths during synchronization.
                    // Reject a missing provider before allocating any material instances.
                    if (!resources)
                        for (const auto* binding : {&item.instance.baseColorTexture,
                                                    &item.instance.normalTexture,
                                                    &item.instance.emissiveTexture,
                                                    &item.instance.metallicRoughnessTexture,
                                                    &item.instance.ambientOcclusionTexture})
                            if (binding->source == MaterialTextureSource::AssetPath && !binding->path.empty())
                                throw std::runtime_error("Texture realization requires a resource provider");
                }
                pending.emplace(key, std::move(item));
            }
        }
        // Stage conversion/IO first; publish associations only after the complete set succeeds.
        for (auto& [key, item] : pending)
        {
            if (item.handle.valid())
                continue;
            context     = identity + ": " + key;
            item.handle = MaterialAssetRealization::createInstance(std::move(item.instance), item.family, runtime);
            allocated.push_back(item.handle);
            if (resources)
            {
                const auto sync = runtime.synchronizeGpuState(item.handle, resources);
                if (!sync.state || !sync.state->textures.baseColor || !sync.state->textures.normal ||
                    !sync.state->textures.metallicRoughness || !sync.state->textures.ambientOcclusion ||
                    !sync.state->textures.emissive)
                    throw std::runtime_error("Material texture/fallback realization failed");
            }
        }
        auto result      = std::make_shared<RuntimeModelMaterials>();
        result->model    = model;
        result->runtime  = &runtime;
        result->lifetime = lifetime;
        for (const auto& ranges : keys)
        {
            auto& bindings = result->bindings.emplace_back();
            for (const auto& key : ranges)
                bindings.push_back(key.empty() ? runtime.defaultMaterial() : pending.at(key).handle);
        }
        for (const auto& [key, item] : pending)
        {
            if (item.slot)
                canonicalHandles[model->model][*item.slot] = item.handle;
            else
                externalHandles[key] = item.handle;
        }
        if (resources)
        {
            const auto sync = runtime.synchronizeGpuState(runtime.defaultMaterial(), resources);
            if (!sync.state || !sync.state->textures.baseColor || !sync.state->textures.normal ||
                !sync.state->textures.metallicRoughness || !sync.state->textures.ambientOcclusion ||
                !sync.state->textures.emissive)
                throw std::runtime_error("Default material texture realization failed");
        }
        GtsModelMaterialResult success{result, {}};
        models[model] = success;
        return success;
    }
    catch (const std::exception& error)
    {
        for (auto handle : allocated)
        {
            auto definition = runtime.getInstance(handle)->definition;
            runtime.destroyInstance(handle);
            runtime.destroyDefinition(definition);
        }
        return {nullptr, {{GtsModelDiagnosticSeverity::Error, "model.material.realization", error.what(), context}}};
    }
}

namespace
{
    struct GtsModelMaterialServiceComponent
    {
        std::shared_ptr<GtsModelMaterialRealization> service;
    };
} // namespace
GtsModelMaterialRealization& modelMaterialRealization(ECSWorld& world, IResourceProvider* resources)
{
    if (!world.hasAny<GtsModelMaterialServiceComponent>())
        world.createSingleton<GtsModelMaterialServiceComponent>();
    auto& service = world.getSingleton<GtsModelMaterialServiceComponent>().service;
    auto& runtime = gts::rendering::materialRuntime(world);
    if (!service || !service->belongsTo(runtime, resources))
        service = std::make_shared<GtsModelMaterialRealization>(runtime, resources);
    return *service;
}
