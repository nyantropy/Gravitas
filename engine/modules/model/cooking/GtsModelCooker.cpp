#include "model/serialization/ModelAssetSerializer.h"
#include "model/cooking/GtsModelCooker.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <fstream>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <system_error>
#include <utility>

#include "assets/serialization/AssetSerializers.h"
#include "model/processing/image/GtsScalarImagePacking.h"
#include "model/import/obj/GtsObjModelImporter.h"
#include "model/import/gltf/GtsGltfModelImporter.h"
#include "assets/importer/image/GtsImageDecode.h"
#include "model/processing/geometry/static/GtsStaticMeshPreparation.h"
#include "model/import/GtsModelImportResult.h"
#include "model/domain/model/GtsModelAsset.h"
#include "model/domain/model/GtsModelValidation.h"
#include "assets/cooking/TextureCooker.h"

#include "assets/cooking/AssetCookingSupport.h"
namespace gts::rendering
{
    using namespace cooking_detail;
    void addModelDependencies(ModelAssetData& model)
    {
        for (const AssetReference& mesh : model.meshes)
            addUniqueReference(model.dependencies, mesh);
        for (const AssetReference& material : model.materials)
            addUniqueReference(model.dependencies, material);
    }

    // Storage capability policy is separate from canonical-domain validity.
    static void validateCookedV1ModelSupport(const GtsModelAsset&         model,
                                             GtsModelCookResult&          result,
                                             const std::filesystem::path& sourcePath)
    {
        if (!model.skinBindings.empty() || !model.skeletonUses.empty())
            addCookDiagnostic(result,
                              AssetDiagnosticSeverity::Error,
                              "ASSET_COOK_V1_CAPABILITY",
                              "Cooked-v1 stores static models only; skin bindings/skeleton uses require an animated "
                              "cooked format. Use canonical source loading.",
                              sourcePath);
        for (const auto& mesh : model.meshes)
            for (const auto& primitive : mesh.primitives)
                for (const auto& attribute : primitive.attributes)
                    if (attribute.semantic == GtsVertexSemantic::Joints ||
                        attribute.semantic == GtsVertexSemantic::Weights)
                        addCookDiagnostic(result,
                                          AssetDiagnosticSeverity::Error,
                                          "ASSET_COOK_V1_CAPABILITY",
                                          "Cooked-v1 cannot preserve weighted geometry in mesh " + mesh.name,
                                          sourcePath);
        if (model.meshes.empty())
            addCookDiagnostic(result,
                              AssetDiagnosticSeverity::Error,
                              "ASSET_COOK_NO_MESHES",
                              "This cooking target requires model geometry",
                              sourcePath);
        // v1 has only UV0; reject image bindings that preparation cannot realize.
        auto checkBinding = [&](const GtsModelImageBinding& binding)
        {
            if (binding.texCoordSet != 0)
                addCookDiagnostic(result,
                                  AssetDiagnosticSeverity::Error,
                                  "ASSET_COOK_UV_SET_UNSUPPORTED",
                                  "Cooked v1 materials sample UV0 only; use UV0 or a future material storage profile",
                                  sourcePath);
        };
        for (const auto& material : model.materials)
        {
            for (const auto& binding : {material.baseColorImage, material.normalImage, material.emissiveImage})
                if (binding)
                    checkBinding(*binding);
            for (const auto& binding :
                 {material.metallicImage, material.roughnessImage, material.ambientOcclusionImage})
                if (binding)
                    checkBinding(binding->image);
        }
    }

    static GtsModelCookResult planModelAsset(const GtsModelAsset&         model,
                                             const std::filesystem::path& sourcePath,
                                             const GtsModelCookerOptions& options)
    {
        GtsModelCookResult result;
        const auto         validation = validateGtsModelAsset(model);
        for (const auto& diagnostic : validation.diagnostics)
            addCookDiagnostic(result,
                              AssetDiagnosticSeverity::Error,
                              diagnostic.code,
                              diagnostic.location + ": " + diagnostic.message,
                              sourcePath);
        validateCookedV1ModelSupport(model, result, sourcePath);
        if (result.hasErrors())
            return result;

        // Only the genuinely single, identity-root representation can omit a model file.
        const bool flat = model.meshes.size() == 1 && model.nodes.size() == 1 && model.nodes[0].meshIndex == 0 &&
                          model.nodes[0].localTransform == glm::mat4(1.0f);
        for (size_t meshIndex = 0; meshIndex < model.meshes.size(); ++meshIndex)
        {
            const auto prepared = prepareGtsStaticMesh(model.meshes[meshIndex]);
            for (const auto& diagnostic : prepared.diagnostics())
                addCookDiagnostic(
                    result,
                    diagnostic.severity == GtsModelDiagnosticSeverity::Error ? AssetDiagnosticSeverity::Error
                                                                             : AssetDiagnosticSeverity::Warning,
                    diagnostic.code,
                    "mesh[" + std::to_string(meshIndex) + "] " + diagnostic.location + ": " + diagnostic.message,
                    sourcePath);
            if (!prepared.succeeded())
                return result;
            const auto&   geometry = *prepared.mesh();
            MeshAssetData mesh;
            mesh.debugName         = model.meshes[meshIndex].name;
            mesh.vertices          = geometry.vertices;
            mesh.indices           = geometry.indices;
            mesh.attributes        = geometry.metadata.attributes;
            mesh.generatedNormals  = geometry.metadata.generatedNormals;
            mesh.generatedTangents = geometry.metadata.generatedTangents;
            mesh.bounds            = computeAssetBounds(mesh.vertices);
            for (const auto& primitive : geometry.primitives)
                mesh.submeshes.push_back({primitive.firstIndex,
                                          primitive.indexCount,
                                          {},
                                          mesh.debugName + "_primitive_" + std::to_string(mesh.submeshes.size())});
            if (mesh.generatedNormals)
                addCookDiagnostic(result,
                                  AssetDiagnosticSeverity::Warning,
                                  "ASSET_COOK_GENERATED_NORMALS",
                                  "mesh[" + std::to_string(meshIndex) +
                                      "]: static preparation generated missing normals",
                                  sourcePath);
            if (mesh.generatedTangents)
                addCookDiagnostic(result,
                                  AssetDiagnosticSeverity::Warning,
                                  "ASSET_COOK_GENERATED_TANGENTS",
                                  "mesh[" + std::to_string(meshIndex) +
                                      "]: static preparation generated missing tangents",
                                  sourcePath);
            if (!options.vertexColorOnly && std::any_of(mesh.vertices.begin(),
                                                        mesh.vertices.end(),
                                                        [](const auto& vertex)
                                                        {
                                                            return vertex.color != glm::vec4(1);
                                                        }))
                addCookDiagnostic(result,
                                  AssetDiagnosticSeverity::Warning,
                                  "ASSET_COOK_VERTEX_COLORS_PRESENT",
                                  "mesh[" + std::to_string(meshIndex) +
                                      "]: vertexColorOnly remains an explicit cooking option",
                                  sourcePath);
            result.meshes.push_back(std::move(mesh));
        }

        const auto       outputDirectory = outputDirectoryFor(sourcePath, options);
        const auto       sourceStem      = sanitizedName(sourcePath.stem().string(), "asset");
        TextureCookCache cache(outputDirectory, sourceStem, options, result, sourcePath);
        // Reuse the existing image decoder/mip machinery, never legacy model DTOs.
        auto imageInput = [&](uint32_t index)
        {
            const auto&      image = model.images[index];
            TextureCookInput texture;
            texture.debugName = image.name;
            if (const auto* path = std::get_if<std::filesystem::path>(&image.source))
            {
                texture.source     = TextureCookSource::ExternalFile;
                texture.sourcePath = *path;
            }
            else
            {
                const auto& embedded  = std::get<GtsModelEmbeddedImage>(image.source);
                texture.source        = TextureCookSource::EmbeddedBytes;
                texture.embeddedBytes = embedded.bytes;
                texture.logicalPath   = sourceStem + "_image_" + std::to_string(index);
            }
            return texture;
        };
        auto cookImage =
            [&](const std::optional<GtsModelImageBinding>& binding, MaterialTextureRole role, const std::string& name)
        {
            if (!binding)
                return AssetReference{};
            return cache.cookTexture(imageInput(binding->imageIndex),
                                     cookRoleForMaterialRole(role),
                                     materialRoleSuffix(role),
                                     name,
                                     sourcePath);
        };
        auto decode = [&](const GtsModelScalarImageBinding& binding) -> std::optional<TextureCookInput>
        {
            auto        texture = imageInput(binding.image.imageIndex);
            std::string error;
            const bool  loaded = decodeTextureInput(texture, &error);
            if (!loaded)
            {
                addCookDiagnostic(result,
                                  AssetDiagnosticSeverity::Error,
                                  "ASSET_COOK_SCALAR_IMAGE_FAILED",
                                  "Cannot decode scalar image: " + error,
                                  sourcePath);
                return std::nullopt;
            }
            return texture;
        };
        auto scalarKey = [](const std::optional<GtsModelScalarImageBinding>& binding)
        {
            return binding ? std::to_string(binding->image.imageIndex) + "_" +
                                 std::to_string(static_cast<int>(binding->channel))
                           : "none";
        };
        auto cookScalars = [&](const std::optional<GtsModelScalarImageBinding>& first,
                               const std::optional<GtsModelScalarImageBinding>& second,
                               bool                                             metallicRoughness,
                               const std::string&                               name)
        {
            if (!first && !second)
                return AssetReference{};
            const auto role =
                metallicRoughness ? TextureCookRole::MetallicRoughness : TextureCookRole::AmbientOcclusion;
            const auto identity = "scalar:" + scalarKey(first) + ":" + scalarKey(second);
            const auto key      = identity + "|role=" + std::to_string(static_cast<uint16_t>(role));
            if (auto found = cache.referencesByKey.find(key); found != cache.referencesByKey.end())
                return found->second;
            const auto a = first ? decode(*first) : std::nullopt;
            const auto b = second ? decode(*second) : std::nullopt;
            if ((first && !a) || (second && !b))
                return AssetReference{};
            if (a && b && (a->width != b->width || a->height != b->height))
            {
                addCookDiagnostic(result,
                                  AssetDiagnosticSeverity::Error,
                                  "ASSET_COOK_SCALAR_IMAGE_SIZE",
                                  "Metallic and roughness images must have equal dimensions for v1 packing; resample "
                                  "them before cooking",
                                  sourcePath);
                return AssetReference{};
            }
            TextureCookInput packed;
            packed.width              = a ? a->width : b->width;
            packed.height             = a ? a->height : b->height;
            packed.sourceChannelCount = 4;
            packed.debugName          = (!first || !second || first->image.imageIndex == second->image.imageIndex)
                                            ? textureOutputStemFor(a ? *a : *b, sourceStem + "_" + name)
                                            : sourceStem + "_" + name;
            packed.logicalPath        = packed.debugName;
            std::array<std::optional<GtsScalarImageChannel>, 4> channels;
            if (a)
                channels[metallicRoughness ? 2 : 0] =
                    GtsScalarImageChannel{a->width, a->height, a->rgba8Pixels, static_cast<uint32_t>(first->channel)};
            if (b)
                channels[1] =
                    GtsScalarImageChannel{b->width, b->height, b->rgba8Pixels, static_cast<uint32_t>(second->channel)};
            GtsDecodedImage pixels;
            std::string     packingError;
            if (!packGtsScalarImages(channels, pixels, &packingError))
            {
                addCookDiagnostic(
                    result, AssetDiagnosticSeverity::Error, "ASSET_COOK_SCALAR_IMAGE_SIZE", packingError, sourcePath);
                return AssetReference{};
            }
            packed.rgba8Pixels   = std::move(pixels.rgba8Pixels);
            const auto suffix    = metallicRoughness ? "_metallic_roughness" : "_ao";
            const auto reference = cache.cookTexture(std::move(packed), role, suffix, name, sourcePath, identity);
            // Preserve external source provenance for each input of a derived scalar texture.
            for (auto& texture : result.textures)
                if (texture.id == reference.id)
                    for (const auto* input : {a ? &*a : nullptr, b ? &*b : nullptr})
                        if (input && !input->sourcePath.empty())
                            addUniqueReference(texture.dependencies,
                                               AssetReference::fromLogicalPath(input->sourcePath.generic_string()));
            return reference;
        };

        std::vector<AssetReference> materialReferences;
        std::set<std::string>       materialNames;
        auto                        cookMaterial = [&](const GtsModelMaterial& imported, bool isDefault)
        {
            std::string name = sanitizedName(imported.name, "material_" + std::to_string(materialReferences.size()));
            const auto  baseName = name;
            for (uint32_t i = 1; !materialNames.insert(name).second; ++i)
                name = baseName + "_" + std::to_string(i);
            const auto        path      = outputDirectory / (sourceStem + "_" + name + ".gmat");
            const auto        reference = referenceForCookedOutput(path);
            MaterialAssetData material;
            material.id                       = reference.id;
            material.debugName                = imported.name;
            material.baseColor                = imported.baseColor;
            material.metallic                 = imported.metallic;
            material.roughness                = imported.roughness;
            material.normalScale              = imported.normalScale;
            material.ambientOcclusionStrength = imported.ambientOcclusionStrength;
            material.emissiveFactor           = imported.emissiveFactor;
            material.emissiveStrength         = imported.emissiveStrength;
            switch (imported.alphaMode)
            {
            case GtsModelAlphaMode::Opaque:
                material.renderState.alphaMode = MaterialAlphaMode::Opaque;
                break;
            case GtsModelAlphaMode::Mask:
                material.renderState.alphaMode = MaterialAlphaMode::Mask;
                break;
            case GtsModelAlphaMode::Blend:
                material.renderState.alphaMode = MaterialAlphaMode::Blend;
                break;
            }
            material.renderState.alphaCutoff = imported.alphaCutoff;
            material.renderState.doubleSided = imported.doubleSided;
            material.renderState.depthWrite  = imported.alphaMode != GtsModelAlphaMode::Blend;
            material.vertexColorOnly         = options.vertexColorOnly;
            material.shaderFamily =
                options.vertexColorOnly ? MaterialShaderFamily::Unlit : MaterialShaderFamily::StandardSurface;
            material.baseColorTexture = cookImage(imported.baseColorImage, MaterialTextureRole::BaseColor, name);
            if (isDefault && !options.baseColorTextureOverride.empty())
                material.baseColorTexture = cache.cookTexture(textureInputForPath(options.baseColorTextureOverride),
                                                              TextureCookRole::BaseColor,
                                                              "",
                                                              name,
                                                              sourcePath);
            material.normalTexture   = cookImage(imported.normalImage, MaterialTextureRole::Normal, name);
            material.emissiveTexture = cookImage(imported.emissiveImage, MaterialTextureRole::Emissive, name);
            material.metallicRoughnessTexture =
                cookScalars(imported.metallicImage, imported.roughnessImage, true, name);
            material.ambientOcclusionTexture = cookScalars(imported.ambientOcclusionImage, std::nullopt, false, name);
            addMaterialTextureDependencies(material);
            if (material.baseColorTexture.empty())
                addCookDiagnostic(result,
                                  AssetDiagnosticSeverity::Warning,
                                  "ASSET_COOK_DEFAULT_BASE_COLOR_TEXTURE",
                                  "Cooked material uses the runtime fallback base color texture",
                                  sourcePath);
            result.materials.push_back(std::move(material));
            materialReferences.push_back(reference);
            result.outputs.push_back({CookedAssetOutputType::Material, path, reference});
            return reference;
        };
        for (const auto& material : model.materials)
            cookMaterial(material, false);
        bool unassigned = model.materials.empty();
        for (const auto& sourceMesh : model.meshes)
            for (const auto& primitive : sourceMesh.primitives)
                unassigned |= !primitive.materialIndex.has_value();
        AssetReference defaultMaterial;
        if (unassigned)
        {
            GtsModelMaterial material;
            material.name = options.vertexColorOnly ? "vertex_color" : "default";
            if (!model.materials.empty())
                material.name += "_unassigned";
            defaultMaterial = cookMaterial(material, true);
            addCookDiagnostic(result,
                              AssetDiagnosticSeverity::Warning,
                              "ASSET_COOK_DEFAULT_MATERIAL",
                              "Cooker emitted a default material for unassigned primitives",
                              sourcePath);
        }
        if (result.hasErrors())
            return result;

        std::vector<AssetReference> meshReferences;
        std::set<std::string>       meshNames;
        for (size_t i = 0; i < result.meshes.size(); ++i)
        {
            auto&       mesh = result.meshes[i];
            std::string name =
                model.meshes.size() == 1
                    ? sourceStem
                    : sourceStem + "_" + sanitizedName(model.meshes[i].name, "mesh_" + std::to_string(i));
            const auto base = name;
            for (uint32_t suffix = 1; !meshNames.insert(name).second; ++suffix)
                name = base + "_" + std::to_string(suffix);
            const auto path      = outputDirectory / (name + ".gmesh");
            const auto reference = referenceForCookedOutput(path);
            mesh.id              = reference.id;
            if (mesh.debugName.empty())
                mesh.debugName = name;
            for (size_t j = 0; j < mesh.submeshes.size(); ++j)
            {
                const auto slot            = model.meshes[i].primitives[j].materialIndex;
                mesh.submeshes[j].material = slot ? materialReferences[*slot] : defaultMaterial;
                addUniqueReference(mesh.dependencies, mesh.submeshes[j].material);
            }
            meshReferences.push_back(reference);
            result.outputs.push_back({CookedAssetOutputType::Mesh, path, reference});
        }
        if (!flat)
        {
            const auto     path      = outputDirectory / (sourceStem + ".gmodel");
            const auto     reference = referenceForCookedOutput(path);
            ModelAssetData cooked;
            cooked.id        = reference.id;
            cooked.debugName = sourceStem;
            cooked.meshes    = meshReferences;
            cooked.materials = materialReferences;
            cooked.nodes.resize(model.nodes.size());
            for (size_t i = 0; i < model.nodes.size(); ++i)
            {
                const auto& node               = model.nodes[i];
                cooked.nodes[i].name           = node.name;
                cooked.nodes[i].localTransform = node.localTransform;
                if (node.meshIndex)
                    cooked.nodes[i].mesh = meshReferences[*node.meshIndex];
                for (const auto child : node.children)
                    cooked.nodes[child].parentIndex = static_cast<int32_t>(i);
            }
            addModelDependencies(cooked);
            result.models.push_back(std::move(cooked));
            result.outputs.push_back({CookedAssetOutputType::Model, path, reference});
        }
        return result;
    }

    // Encode the complete plan before touching the destination. Publish the model entry last.
    // Stage in the destination filesystem; roll back replacements on reported I/O failure.
    // This protects a package from ordinary failures, not concurrent writers or process crashes.
    static void publishCookedPackage(GtsModelCookResult& result, const std::filesystem::path& sourcePath)
    {
        if (result.hasErrors())
        {
            result.outputs.clear();
            return;
        }
        std::vector<std::vector<uint8_t>> bytes(result.outputs.size());
        size_t                            mesh = 0, material = 0, texture = 0, model = 0;
        for (size_t i = 0; i < result.outputs.size(); ++i)
        {
            std::string error;
            bool        ok = false;
            switch (result.outputs[i].type)
            {
            case CookedAssetOutputType::Mesh:
                ok = MeshAssetSerializer::serialize(result.meshes[mesh++], bytes[i], &error);
                break;
            case CookedAssetOutputType::Material:
                ok = MaterialAssetSerializer::serialize(result.materials[material++], bytes[i], &error);
                break;
            case CookedAssetOutputType::Texture:
                ok = TextureAssetSerializer::serialize(result.textures[texture++], bytes[i], &error);
                break;
            case CookedAssetOutputType::Model:
                ok = ModelAssetSerializer::serialize(result.models[model++], bytes[i], &error);
                break;
            default:
                break;
            }
            if (!ok)
            {
                addCookDiagnostic(
                    result, AssetDiagnosticSeverity::Error, "ASSET_COOK_SERIALIZATION", error, sourcePath);
                result.outputs.clear();
                return;
            }
        }
        publishCookedFiles(result, sourcePath, bytes);
    }

    GtsModelCookResult GtsModelCooker::cookModelAsset(const GtsModelAsset&         model,
                                                      const std::filesystem::path& sourcePath,
                                                      const GtsModelCookerOptions& options)
    {
        auto result = planModelAsset(model, sourcePath, options);
        publishCookedPackage(result, sourcePath);
        return result;
    }

    GtsModelCookResult GtsModelCooker::cookModelBundle(const GtsModelImportBundle&  bundle,
                                                       const std::filesystem::path& sourcePath,
                                                       const GtsModelCookerOptions& options)
    {
        GtsModelCookResult result;
        if (!bundle.model)
            addCookDiagnostic(result,
                              AssetDiagnosticSeverity::Error,
                              "ASSET_COOK_MODEL_REQUIRED",
                              "This cooking target requires a model",
                              sourcePath);
        if (!bundle.skeletons.empty() || !bundle.animationClips.empty())
            addCookDiagnostic(result,
                              AssetDiagnosticSeverity::Error,
                              "ASSET_COOK_V1_CAPABILITY",
                              "Cooked-v1 cannot preserve skeleton definitions or animation clips; use canonical source "
                              "loading until an animated cooked format exists",
                              sourcePath);
        if (result.hasErrors())
            return result;
        result = cookModelAsset(*bundle.model, sourcePath, options);
        return result;
    }

    GtsModelCookResult GtsModelCooker::cookSourceAsset(const std::filesystem::path& sourcePath,
                                                       const GtsModelCookerOptions& options)
    {
        std::string extension = sourcePath.extension().string();
        std::transform(extension.begin(),
                       extension.end(),
                       extension.begin(),
                       [](unsigned char ch)
                       {
                           return static_cast<char>(std::tolower(ch));
                       });
        if (options.explicitImporter == "obj" || options.explicitImporter == "gltf" ||
            (options.explicitImporter.empty() && (extension == ".obj" || extension == ".gltf" || extension == ".glb")))
        {
            std::unique_ptr<IGtsModelImporter> importer;
            if (options.explicitImporter == "obj" || (options.explicitImporter.empty() && extension == ".obj"))
                importer = std::make_unique<GtsObjModelImporter>();
            else
                importer = std::make_unique<GtsGltfModelImporter>();
            const auto         imported = importer->importAsset({sourcePath});
            GtsModelCookResult result;
            if (imported.succeeded())
                result = cookModelBundle(*imported.bundle(), sourcePath, options);
            for (const auto& diagnostic : imported.diagnostics())
                addCookDiagnostic(result,
                                  diagnostic.severity == GtsModelDiagnosticSeverity::Error
                                      ? AssetDiagnosticSeverity::Error
                                      : AssetDiagnosticSeverity::Warning,
                                  diagnostic.code,
                                  diagnostic.location + ": " + diagnostic.message,
                                  sourcePath);
            return result;
        }

        GtsModelCookResult result;
        addCookDiagnostic(result,
                          AssetDiagnosticSeverity::Error,
                          "ASSET_IMPORTER_NOT_FOUND",
                          "No canonical model importer or image decoder for source: " + sourcePath.string(),
                          sourcePath);
        return result;
    }
} // namespace gts::rendering
