#include "../../assets/importers/gltf/GltfFixtureBuilder.h"
#include "../../assets/runtime/ScopedRuntimeAssetPolicy.h"
#include "assets/loading/model/GtsModelRegistry.h"
#include "assets/loading/model/GtsModelResource.h"
#include "assets/realization/model/GtsModelRealization.h"
#include "assets/serialization/AssetSerializers.h"
#include "rendering/core/material/model/GtsModelMaterialRealization.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STB_IMAGE_WRITE_STATIC
#include <stb_image_write.h>

using namespace gts::rendering;
namespace
{
    struct Resources : IResourceProvider
    {
        struct Upload
        {
            std::shared_ptr<const GtsDecodedImage> image;
            TextureColorSpace                      space;
            texture_id_type                        id;
        };
        std::vector<Upload>                    uploads;
        std::map<std::string, texture_id_type> paths;
        texture_id_type                        next         = 10;
        bool                                   fail         = false;
        bool                                   failFallback = false;
        texture_id_type                        requestMemoryTexture(std::shared_ptr<const GtsDecodedImage> image,
                                                                    TextureColorSpace                      space) override
        {
            if (fail)
                return 0;
            uploads.push_back({image, space, next});
            return next++;
        }
        texture_id_type requestTexture(const std::string& path) override
        {
            return requestTexture(path, TextureColorSpace::SRgb);
        }
        texture_id_type requestTexture(const std::string& path, TextureColorSpace space) override
        {
            if (!path.empty() && !std::filesystem::exists(path))
                throw std::runtime_error("missing texture: " + path);
            auto key            = path + std::to_string(static_cast<int>(space));
            auto [it, inserted] = paths.emplace(key, next);
            if (inserted)
                ++next;
            return fail ? 0 : it->second;
        }
        texture_id_type requestMaterialFallbackTexture(MaterialTextureRole role) override
        {
            return failFallback ? 0 : 1 + static_cast<int>(role);
        }
        mesh_id_type requestMesh(const std::string&) override
        {
            return 0;
        }
        mesh_id_type getSharedQuadMesh(float, float) override
        {
            return 0;
        }
        mesh_id_type uploadProceduralMesh(mesh_id_type,
                                          const std::vector<GtsStaticVertex>&,
                                          const std::vector<uint32_t>&,
                                          VertexAttributeFlags) override
        {
            return 0;
        }
        void            releaseProceduralMesh(mesh_id_type) override {}
        texture_id_type requestClampedTexture(const std::string& p) override
        {
            return requestTexture(p);
        }
        texture_id_type requestPixelTexture(const std::string& p) override
        {
            return requestTexture(p);
        }
        TextureDimensions getTextureDimensions(texture_id_type) const override
        {
            return {};
        }
        font_id_type requestFont(const std::string&) override
        {
            return 0;
        }
        const BitmapFont* getFont(font_id_type) const override
        {
            return nullptr;
        }
        view_id_type requestCameraBuffer() override
        {
            return 0;
        }
        void         releaseCameraBuffer(view_id_type) override {}
        void         uploadCameraView(view_id_type, const glm::mat4&, const glm::mat4&) override {}
        ssbo_id_type requestObjectSlot() override
        {
            return 0;
        }
        void          releaseObjectSlot(ssbo_id_type) override {}
        const Upload& upload(texture_id_type id) const
        {
            for (const auto& item : uploads)
                if (item.id == id)
                    return item;
            throw std::runtime_error("missing captured upload");
        }
    };
    std::shared_ptr<const GtsRealizedModel> realize(GtsModelRegistry& registry, const std::filesystem::path& path)
    {
        auto loaded = registry.requestModel(path);
        if (!loaded.succeeded())
            throw std::runtime_error(loaded.diagnostics().back().message);
        auto result = realizeGtsModel(loaded.handle());
        if (!result.succeeded())
            throw std::runtime_error(result.diagnostics().back().message);
        return result.model();
    }
    std::shared_ptr<const GtsRealizedModel>
    fixture(GtsModelRegistry& registry, GltfFixtureBuilder f, const std::filesystem::path& dir)
    {
        std::filesystem::create_directories(dir);
        return realize(registry, f.write(dir, GltfFixtureFormat::Glb));
    }
    void png(const std::filesystem::path& path, int width = 1)
    {
        std::vector<uint8_t> pixels;
        for (int i = 0; i < width; ++i)
            pixels.insert(pixels.end(), {31, 63, 127, 191});
        require(stbi_write_png(path.string().c_str(), width, 1, 4, pixels.data(), width * 4), "write PNG");
    }
    void factors(const std::filesystem::path& root)
    {
        GtsModelRegistry   registry;
        GltfFixtureBuilder f;
        field(f.root, "materials") = parse(
            R"([{"pbrMetallicRoughness":{"baseColorFactor":[0.2,0.3,0.4,0.6],"metallicFactor":0.7,"roughnessFactor":0.8},"emissiveFactor":[0.1,0.2,0.3],"extensions":{"KHR_materials_emissive_strength":{"emissiveStrength":3}},"alphaMode":"MASK","alphaCutoff":0.25,"doubleSided":true}])");
        field(f.primitive(), "material") = 0u;
        auto material                    = at(field(f.root, "materials"), 0);
        field(material, "alphaMode")     = "BLEND";
        std::get<Array>(field(f.root, "materials").value).push_back(material);
        auto primitive = f.primitive();
        std::get<Array>(field(at(field(f.root, "meshes"), 0), "primitives").value).push_back(primitive);
        field(primitive, "material") = 1u;
        std::get<Array>(field(at(field(f.root, "meshes"), 0), "primitives").value).push_back(primitive);
        // A distinct slot with identical values must remain independently addressable.
        std::get<Array>(field(f.root, "materials").value).push_back(at(field(f.root, "materials"), 0));
        field(primitive, "material") = 2u;
        std::get<Array>(field(at(field(f.root, "meshes"), 0), "primitives").value).push_back(primitive);
        auto                        model = fixture(registry, f, root / "factors");
        MaterialRuntime             runtime;
        GtsModelMaterialRealization service(runtime, nullptr);
        auto                        result = service.realize(model);
        require(result.succeeded(), "canonical factors");
        auto h = result.materials->materialFor(*model, 0, 0);
        require(h == result.materials->materialFor(*model, 0, 1), "repeated slot reuses handle");
        require(h != result.materials->materialFor(*model, 0, 2), "distinct slots remain distinct");
        require(h != result.materials->materialFor(*model, 0, 3), "equal-value slots remain distinct");
        const auto& instance = *runtime.getInstance(h);
        require(instance.baseColor == glm::vec4(.2f, .3f, .4f, .6f) && instance.metallic == .7f &&
                    instance.roughness == .8f,
                "PBR factors");
        require(instance.emissiveFactor == glm::vec3(.1f, .2f, .3f) && instance.emissiveStrength == 3,
                "emissive factors");
        require(instance.renderState.alphaMode == MaterialAlphaMode::Mask && instance.renderState.alphaCutoff == .25f &&
                    instance.renderState.doubleSided,
                "mask and double-sided");
        const auto* blend = runtime.getInstance(result.materials->materialFor(*model, 0, 2));
        require(blend->renderState.alphaMode == MaterialAlphaMode::Blend && !blend->renderState.depthWrite,
                "blend policy");
        require(service.realize(model).materials == result.materials, "model/runtime cache");
        auto equivalent = realizeGtsModel(model->model).model();
        auto another    = service.realize(equivalent);
        require(another.materials->materialFor(*equivalent, 0, 0) == h,
                "resource slot reuse across equivalent realizations");
        require(!result.materials->materialFor(*equivalent, 0, 0).valid(), "foreign realization association rejected");
        require(!result.materials->materialFor(*model, 0, 90).valid(), "out of range rejected");
        require(result.materials->frameStateFor(*model, 0, 0).parameters.baseColor == instance.baseColor,
                "runtime CPU snapshot shares conversion");
        std::shared_ptr<const GtsRealizedModelMaterials> otherSet;
        {
            MaterialRuntime             otherRuntime;
            GtsModelMaterialRealization other(otherRuntime, nullptr);
            otherSet = other.realize(model).materials;
            require(otherSet && otherSet != result.materials &&
                        otherRuntime.isInstanceAlive(otherSet->materialFor(*model, 0, 0)),
                    "independent world scope");
        }
        require(!otherSet->valid() && !otherSet->materialFor(*model, 0, 0).valid(),
                "expired runtime invalidates association without dereferencing it");
        auto invalid   = std::make_shared<GtsRealizedModel>();
        invalid->model = model->model;
        auto mesh      = std::make_shared<GtsPreparedStaticMesh>();
        mesh->primitives.push_back({0, 3, 99, {}});
        invalid->geometry.emplace_back(std::shared_ptr<const GtsPreparedStaticMesh>(mesh));
        require(!service.realize(invalid).succeeded(), "invalid canonical slot fails");
        require(model->model->canonicalModel()->materials[0].metallic == .7f, "shared data unchanged");
        // Logical association has the same meaning for a skinned profile; no vertex interpretation.
        auto mixed   = std::make_shared<GtsRealizedModel>(*model);
        auto skinned = std::make_shared<GtsPreparedSkinnedMesh>();
        skinned->primitives.push_back({0, 3, 0, {}});
        mixed->geometry.emplace_back(std::shared_ptr<const GtsPreparedSkinnedMesh>(skinned));
        auto mixedSet = service.realize(mixed);
        require(mixedSet.succeeded() && mixedSet.materials->materialFor(*mixed, 1, 0) == h,
                "same logical slot independent of geometry profile");
        ECSWorld world;
        auto&    worldService = modelMaterialRealization(world, nullptr);
        auto     worldSet     = worldService.realize(model).materials;
        require(&worldService == &modelMaterialRealization(world, nullptr), "world service reused");
        resetMaterialRuntime(world);
        require(!worldSet->valid(), "world reset invalidates existing associations");
        require(modelMaterialRealization(world, nullptr).realize(model).succeeded(),
                "world service recreated on reset");
        resetMaterialRuntime(world);
    }
    void defaultsAndRollback(const std::filesystem::path& root)
    {
        GltfFixtureBuilder f;
        field(f.root, "materials")       = parse("[{}]");
        field(f.primitive(), "material") = 0u;
        GtsModelRegistry            registry;
        auto                        model = fixture(registry, f, root / "defaults");
        MaterialRuntime             runtime;
        Resources                   provider;
        GtsModelMaterialRealization service(runtime, &provider);
        provider.failFallback = true;
        auto failed           = service.realize(model);
        require(!failed.succeeded() && !failed.materials, "fallback failure publishes no partial set");
        provider.failFallback = false;
        auto result           = service.realize(model);
        require(result.succeeded(), "failed allocation/sync can be retried");
        auto        handle   = result.materials->materialFor(*model, 0, 0);
        const auto* instance = runtime.getInstance(handle);
        require(instance->baseColor == glm::vec4(1) && instance->metallic == 1 && instance->roughness == 1 &&
                    instance->normalScale == 1 && instance->ambientOcclusionStrength == 1 &&
                    instance->emissiveFactor == glm::vec3(0) && instance->emissiveStrength == 1 &&
                    instance->renderState.alphaMode == MaterialAlphaMode::Opaque && !instance->renderState.doubleSided,
                "default canonical factors use runtime semantics");
        runtime.destroyInstance(handle);
        require(!result.materials->valid(), "destroyed material invalidates cached set");
        require(service.realize(model).succeeded(), "destroyed handle is not reused");
    }
    void textures(const std::filesystem::path& root)
    {
        png(root / "color.png");
        std::ifstream        file(root / "color.png", std::ios::binary);
        std::vector<uint8_t> bytes{std::istreambuf_iterator<char>(file), {}};
        GltfFixtureBuilder   f;
        f.setTexturedMaterial();
        field(at(field(f.root, "images"), 0), "uri") = "data:image/png;base64," + base64(bytes);
        auto& mat                                    = at(field(f.root, "materials"), 0);
        field(field(field(mat, "pbrMetallicRoughness"), "baseColorTexture"), "texCoord") = 0u;
        GtsModelRegistry            registry;
        Resources                   provider;
        MaterialRuntime             runtime;
        GtsModelMaterialRealization service(runtime, &provider);
        auto                        model  = fixture(registry, f, root / "embedded");
        auto                        result = service.realize(model);
        require(result.succeeded(), result.diagnostics.empty() ? "embedded material" : result.diagnostics[0].message);
        auto h        = result.materials->materialFor(*model, 0, 0);
        auto instance = *runtime.getInstance(h);
        require(instance.baseColorTexture.resolvedTextureID == instance.emissiveTexture.resolvedTextureID,
                "same color image reused");
        require(instance.normalTexture.resolvedTextureID != instance.baseColorTexture.resolvedTextureID,
                "color/data interpretations separate");
        require(provider.upload(instance.baseColorTexture.resolvedTextureID).space == TextureColorSpace::SRgb &&
                    provider.upload(instance.normalTexture.resolvedTextureID).space == TextureColorSpace::Linear,
                "role colorspaces");
        require(instance.normalScale == .6f && instance.ambientOcclusionStrength == .4f, "normal and AO factors");
        auto mr = provider.upload(instance.metallicRoughnessTexture.resolvedTextureID).image;
        auto ao = provider.upload(instance.ambientOcclusionTexture.resolvedTextureID).image;
        require(mr->rgba8Pixels == std::vector<uint8_t>({255, 63, 127, 255}) &&
                    ao->rgba8Pixels == std::vector<uint8_t>({31, 255, 255, 255}),
                "MR green/blue and separate AO red packing");
        const auto count = provider.uploads.size();
        service.realize(model);
        require(provider.uploads.size() == count, "no repeated decode/upload/pack");
        auto direct = f;
        // glTF disallows absolute URIs: keep image alongside fixture with relative source reference.
        field(at(field(direct.root, "images"), 0), "uri") = "../color.png";
        auto external                                     = fixture(registry, direct, root / "external");
        auto externalResult                               = service.realize(external);
        require(externalResult.succeeded(), "external imported path");
        require(provider.upload(runtime.getInstance(externalResult.materials->materialFor(*external, 0, 0))
                                    ->baseColorTexture.resolvedTextureID)
                        .image->rgba8Pixels == std::vector<uint8_t>({31, 63, 127, 191}),
                "external decode");
        field(field(field(mat, "pbrMetallicRoughness"), "baseColorTexture"), "texCoord") = 1u;
        auto uv     = fixture(registry, f, root / "uv");
        auto failed = service.realize(uv);
        require(!failed.succeeded() && failed.diagnostics[0].message.find("UV1") != std::string::npos, "no UV remap");
        field(field(field(mat, "pbrMetallicRoughness"), "baseColorTexture"), "texCoord") = 0u;
        field(at(field(f.root, "images"), 0), "uri") = "data:image/png;base64,AQIDBA==";
        require(!service.realize(fixture(registry, f, root / "bad-image")).succeeded(), "decode failure");
        provider.fail = true;
        require(!service.realize(fixture(registry, direct, root / "upload-failure")).succeeded(),
                "texture creation failure exposes no set");
        provider.fail = false;
    }
    void cooked(const std::filesystem::path& root)
    {
        MeshAssetData mesh;
        mesh.vertices.resize(3);
        mesh.vertices[1].pos.x = 1;
        mesh.vertices[2].pos.y = 1;
        mesh.indices           = {0, 1, 2, 0, 1, 2, 0, 1, 2};
        mesh.submeshes         = {{0, 3, AssetReference::fromLogicalPath("surface.gmat"), {}},
                                  {3, 3, AssetReference::fromLogicalPath("surface.gmat"), {}},
                                  {6, 3, {}, {}}};
        MaterialAssetData material;
        material.baseColor    = {.3f, .4f, .5f, 1};
        material.shaderFamily = MaterialShaderFamily::StandardSurface;
        std::string error;
        require(MaterialAssetSerializer::writeFile(material, root / "surface.gmat", &error), error);
        require(MeshAssetSerializer::writeFile(mesh, root / "mesh.gmesh", &error), error);
        GtsModelRegistry            registry;
        MaterialRuntime             runtime;
        GtsModelMaterialRealization service(runtime, nullptr);
        auto                        model  = realize(registry, root / "mesh.gmesh");
        auto                        result = service.realize(model);
        require(result.succeeded(), "cooked material realization");
        auto h = result.materials->materialFor(*model, 0, 0);
        require(h == result.materials->materialFor(*model, 0, 1) &&
                    runtime.getInstance(h)->baseColor == material.baseColor,
                "cooked reference identity reuse");
        require(result.materials->materialFor(*model, 0, 2) == runtime.defaultMaterial(), "world default reused");
        MaterialRuntime             other;
        GtsModelMaterialRealization otherService(other, nullptr);
        std::filesystem::remove(root / "surface.gmat");
        require(!otherService.realize(model).succeeded(), "missing gmat failure");
        std::ofstream(root / "surface.gmat") << "broken";
        require(!otherService.realize(model).succeeded(), "corrupt gmat failure");
        require(MaterialAssetSerializer::writeFile(material, root / "surface.gmat", &error), error);
        require(otherService.realize(model).succeeded(), "failure retry does not poison cache");
        // Resolve texture paths from the material's directory, independent of process cwd.
        material.baseColorTexture = AssetReference::fromLogicalPath("color.png");
        require(MaterialAssetSerializer::writeFile(material, root / "surface.gmat", &error), error);
        MaterialRuntime             texturedRuntime;
        Resources                   provider;
        GtsModelMaterialRealization textured(texturedRuntime, &provider);
        auto                        texturedSet = textured.realize(model);
        require(texturedSet.succeeded(), "cooked texture reference");
        require(provider.paths.contains((root / "color.png").generic_string() +
                                        std::to_string(static_cast<int>(TextureColorSpace::SRgb))),
                "material reference directory");
        material.baseColorTexture = AssetReference::fromLogicalPath("missing.png");
        require(MaterialAssetSerializer::writeFile(material, root / "surface.gmat", &error), error);
        MaterialRuntime             missingRuntime;
        GtsModelMaterialRealization missing(missingRuntime, &provider);
        require(!missing.realize(model).succeeded(), "missing cooked texture fails without partial set");
    }
    void scalarChannels(const std::filesystem::path& root)
    {
        png(root / "scalar.png");
        png(root / "wide.png", 2);
        std::ofstream(root / "scalar.obj")
            << "mtllib scalar.mtl\nv 0 0 0\nv 1 0 0\nv 0 1 0\nvt 0 0\nvt 1 0\nvt 0 1\nusemtl surface\nf 1/1 2/2 3/3\n";
        auto run = [&](const char* maps)
        {
            std::ofstream(root / "scalar.mtl") << "newmtl surface\n" << maps;
            GtsModelRegistry registry;
            return realize(registry, root / "scalar.obj");
        };
        MaterialRuntime             runtime;
        Resources                   provider;
        GtsModelMaterialRealization service(runtime, &provider);
        auto                        metal  = run("map_Pm -imfchan g scalar.png\n");
        auto                        result = service.realize(metal);
        require(result.succeeded(), "metal only");
        auto instance = runtime.getInstance(result.materials->materialFor(*metal, 0, 0));
        require(provider.upload(instance->metallicRoughnessTexture.resolvedTextureID).image->rgba8Pixels ==
                    std::vector<uint8_t>({255, 255, 63, 255}),
                "metal channel selection");
        auto rough = run("map_Pr -imfchan b scalar.png\nmap_Ka -imfchan g scalar.png\n");
        result     = service.realize(rough);
        require(result.succeeded(), "rough only plus AO");
        instance = runtime.getInstance(result.materials->materialFor(*rough, 0, 0));
        require(provider.upload(instance->metallicRoughnessTexture.resolvedTextureID).image->rgba8Pixels ==
                    std::vector<uint8_t>({255, 127, 255, 255}),
                "rough channel selection");
        require(provider.upload(instance->ambientOcclusionTexture.resolvedTextureID).image->rgba8Pixels[0] == 63,
                "AO channel selection");
        auto mismatch = run("map_Pr scalar.png\nmap_Pm wide.png\n");
        result        = service.realize(mismatch);
        require(!result.succeeded() && result.diagnostics[0].message.find("equal dimensions") != std::string::npos,
                "no resampling");
    }
} // namespace
int main()
{
    ScopedRuntimeAssetPolicy policy("development");
    const auto               root = std::filesystem::temp_directory_path() / "gravitas-model-material-test";
    std::filesystem::create_directories(root);
    factors(root);
    defaultsAndRollback(root);
    textures(root);
    cooked(root);
    scalarChannels(root);
    std::filesystem::remove_all(root);
}
