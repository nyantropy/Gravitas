#include "assets/cooking/AssetCooker.h"

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
#include "assets/processing/image/GtsScalarImagePacking.h"
#include "assets/importer/obj/GtsObjModelImporter.h"
#include "assets/importer/gltf/GtsGltfModelImporter.h"
#include "assets/importer/image/GtsImageDecode.h"
#include "assets/processing/geometry/static/GtsStaticMeshPreparation.h"
#include "assets/model/GtsModelImportResult.h"
#include "assets/model/GtsModelAsset.h"
#include "assets/model/GtsModelValidation.h"
#include "assets/cooking/TextureCooker.h"

namespace gts::rendering
{
    namespace
    {
        void addCookDiagnostic(AssetCookResult&             result,
                               AssetDiagnosticSeverity      severity,
                               std::string                  code,
                               std::string                  message,
                               const std::filesystem::path& sourcePath)
        {
            result.diagnostics.push_back({severity, std::move(code), std::move(message), sourcePath, 0});
        }

        std::filesystem::path outputDirectoryFor(const std::filesystem::path& sourcePath,
                                                 const AssetCookerOptions&    options)
        {
            if (!options.outputDirectory.empty())
                return options.outputDirectory;
            if (!sourcePath.parent_path().empty())
                return sourcePath.parent_path();
            return ".";
        }

        std::string sanitizedName(std::string name, std::string fallback)
        {
            if (name.empty())
                name = std::move(fallback);

            for (char& ch : name)
            {
                const unsigned char value = static_cast<unsigned char>(ch);
                if (std::isalnum(value) == 0 && ch != '_' && ch != '-' && ch != '.')
                    ch = '_';
            }

            if (name.empty())
                return "asset";
            return name;
        }

        std::string logicalPathForCookedOutput(const std::filesystem::path& path)
        {
            return path.filename().generic_string();
        }

        AssetReference referenceForCookedOutput(const std::filesystem::path& path)
        {
            return AssetReference::fromLogicalPath(logicalPathForCookedOutput(path));
        }

        bool sameReference(const AssetReference& lhs, const AssetReference& rhs)
        {
            return lhs.id == rhs.id && lhs.logicalPath == rhs.logicalPath;
        }

        void addUniqueReference(std::vector<AssetReference>& references, const AssetReference& reference)
        {
            if (reference.empty())
                return;

            const auto found = std::find_if(references.begin(),
                                            references.end(),
                                            [&reference](const AssetReference& existing)
                                            {
                                                return sameReference(existing, reference);
                                            });
            if (found == references.end())
                references.push_back(reference);
        }

        TextureCookRole cookRoleForMaterialRole(MaterialTextureRole role)
        {
            switch (role)
            {
            case MaterialTextureRole::BaseColor:
                return TextureCookRole::BaseColor;
            case MaterialTextureRole::MetallicRoughness:
                return TextureCookRole::MetallicRoughness;
            case MaterialTextureRole::Normal:
                return TextureCookRole::Normal;
            case MaterialTextureRole::AmbientOcclusion:
                return TextureCookRole::AmbientOcclusion;
            case MaterialTextureRole::Emissive:
                return TextureCookRole::Emissive;
            }
            return TextureCookRole::BaseColor;
        }

        const char* materialRoleSuffix(MaterialTextureRole role)
        {
            switch (role)
            {
            case MaterialTextureRole::BaseColor:
                return "";
            case MaterialTextureRole::MetallicRoughness:
                return "_metallic_roughness";
            case MaterialTextureRole::Normal:
                return "_normal";
            case MaterialTextureRole::AmbientOcclusion:
                return "_ao";
            case MaterialTextureRole::Emissive:
                return "_emissive";
            }
            return "";
        }

        std::string textureIdentityFor(const TextureCookInput& texture)
        {
            if (texture.source == TextureCookSource::ExternalFile && !texture.sourcePath.empty())
                return "file:" + texture.sourcePath.lexically_normal().generic_string();
            if (!texture.logicalPath.empty())
                return "logical:" + texture.logicalPath;
            if (!texture.embeddedBytes.empty())
            {
                uint64_t hash = 1469598103934665603ull;
                for (uint8_t byte : texture.embeddedBytes)
                {
                    hash ^= byte;
                    hash *= 1099511628211ull;
                }
                return "embedded:" + std::to_string(texture.embeddedBytes.size()) + ":" + std::to_string(hash);
            }
            return "debug:" + texture.debugName;
        }

        std::string textureOutputStemFor(const TextureCookInput& texture, const std::string& fallback)
        {
            std::string stem;
            if (texture.source == TextureCookSource::ExternalFile && !texture.sourcePath.empty())
                stem = texture.sourcePath.stem().string();
            if (stem.empty() && !texture.logicalPath.empty())
                stem = std::filesystem::path(texture.logicalPath).stem().string();
            if (stem.empty())
                stem = texture.debugName;
            return sanitizedName(stem, fallback);
        }

        TextureCookInput textureInputForPath(const std::filesystem::path& path)
        {
            TextureCookInput texture;
            texture.debugName   = path.stem().string();
            texture.sourcePath  = path;
            texture.logicalPath = path.filename().generic_string();
            texture.source      = TextureCookSource::ExternalFile;
            return texture;
        }

        void addMaterialTextureDependencies(MaterialAssetData& asset)
        {
            asset.dependencies.clear();
            addUniqueReference(asset.dependencies, asset.baseColorTexture);
            addUniqueReference(asset.dependencies, asset.metallicRoughnessTexture);
            addUniqueReference(asset.dependencies, asset.normalTexture);
            addUniqueReference(asset.dependencies, asset.ambientOcclusionTexture);
            addUniqueReference(asset.dependencies, asset.emissiveTexture);
        }

        bool ensureOutputDirectory(const std::filesystem::path& outputDirectory,
                                   AssetCookResult&             result,
                                   const std::filesystem::path& sourcePath)
        {
            std::error_code errorCode;
            std::filesystem::create_directories(outputDirectory, errorCode);
            if (errorCode)
            {
                addCookDiagnostic(result,
                                  AssetDiagnosticSeverity::Error,
                                  "ASSET_COOK_OUTPUT_DIRECTORY_FAILED",
                                  "Could not create asset output directory: " + outputDirectory.string(),
                                  sourcePath);
                return false;
            }
            return true;
        }

        bool decodeTextureInput(TextureCookInput& input, std::string* error)
        {
            if (input.decoded())
                return true;
            GtsDecodedImage decoded;
            const bool      ok = input.source == TextureCookSource::ExternalFile
                                     ? decodeGtsImage(input.sourcePath, decoded, error)
                                     : decodeGtsImage(std::span<const uint8_t>(input.embeddedBytes), decoded, error);
            if (!ok)
                return false;
            input.width              = decoded.width;
            input.height             = decoded.height;
            input.sourceChannelCount = decoded.sourceChannelCount;
            input.rgba8Pixels        = std::move(decoded.rgba8Pixels);
            return true;
        }

        struct TextureCookCache
        {
            const std::filesystem::path& outputDirectory;
            const std::string&           sourceStem;
            const AssetCookerOptions&    options;
            AssetCookResult&             result;
            const std::filesystem::path& sourcePath;

            std::map<std::string, AssetReference>  referencesByKey;
            std::map<std::string, std::string>     outputOwners;
            std::map<std::string, TextureCookRole> firstRoleByIdentity;
            std::set<std::string>                  reportedRoleConflicts;

            TextureCookCache(const std::filesystem::path& outputDirectory,
                             const std::string&           sourceStem,
                             const AssetCookerOptions&    options,
                             AssetCookResult&             result,
                             const std::filesystem::path& sourcePath)
                : outputDirectory(outputDirectory), sourceStem(sourceStem), options(options), result(result),
                  sourcePath(sourcePath)
            {
            }

            AssetReference cookTexture(TextureCookInput             texture,
                                       TextureCookRole              role,
                                       const std::string&           outputSuffix,
                                       const std::string&           fallbackName,
                                       const std::filesystem::path& diagnosticSource,
                                       const std::string&           identityOverride = {})
            {
                const std::string identity  = identityOverride.empty() ? textureIdentityFor(texture) : identityOverride;
                const auto        roleFound = firstRoleByIdentity.find(identity);
                if (roleFound == firstRoleByIdentity.end())
                {
                    firstRoleByIdentity.emplace(identity, role);
                }
                else if (roleFound->second != role && reportedRoleConflicts.insert(identity).second)
                {
                    addCookDiagnostic(result,
                                      AssetDiagnosticSeverity::Warning,
                                      "TEXTURE_COLOR_SPACE_CONFLICT",
                                      "The same source texture is used with multiple texture roles; cooker emitted "
                                      "separate cooked variants",
                                      diagnosticSource);
                }

                const std::string key            = identity + "|role=" + std::to_string(static_cast<uint16_t>(role));
                const auto        referenceFound = referencesByKey.find(key);
                if (referenceFound != referencesByKey.end())
                    return referenceFound->second;

                if (!decodeTexture(texture, diagnosticSource))
                    return {};

                const std::string baseStem       = textureOutputStemFor(texture, sourceStem + "_" + fallbackName);
                std::string       fileStem       = baseStem + outputSuffix;
                std::string       fileName       = fileStem + ".gtex";
                uint32_t          collisionIndex = 1;
                while (true)
                {
                    const auto ownerFound = outputOwners.find(fileName);
                    if (ownerFound == outputOwners.end() || ownerFound->second == key)
                        break;
                    fileName = fileStem + "_" + std::to_string(collisionIndex++) + ".gtex";
                }
                outputOwners[fileName] = key;

                const std::filesystem::path outputPath = outputDirectory / fileName;
                const AssetReference        reference  = referenceForCookedOutput(outputPath);

                TextureCookerOptions textureOptions;
                textureOptions.role            = role;
                textureOptions.generateMipmaps = options.generateTextureMipmaps;
                textureOptions.sampler         = defaultSamplerForTextureCookRole(role, options.generateTextureMipmaps);
                textureOptions.debugName       = baseStem;

                TextureCookResult cookResult =
                    TextureCooker::cookTexture(texture, reference.id, textureOptions, diagnosticSource);
                result.diagnostics.insert(
                    result.diagnostics.end(), cookResult.diagnostics.begin(), cookResult.diagnostics.end());
                if (cookResult.hasErrors())
                    return {};

                result.textures.push_back(std::move(cookResult.texture));

                result.outputs.push_back({CookedAssetOutputType::Texture, outputPath, reference});
                referencesByKey.emplace(key, reference);
                return reference;
            }

            private:
            bool decodeTexture(TextureCookInput& texture, const std::filesystem::path& diagnosticSource)
            {
                std::string error;
                if (decodeTextureInput(texture, &error))
                    return true;
                addCookDiagnostic(
                    result, AssetDiagnosticSeverity::Error, "TEXTURE_DECODE_FAILED", error, diagnosticSource);
                return false;
            }
        };

        void addModelDependencies(ModelAssetData& model)
        {
            for (const AssetReference& mesh : model.meshes)
                addUniqueReference(model.dependencies, mesh);
            for (const AssetReference& material : model.materials)
                addUniqueReference(model.dependencies, material);
        }
    } // namespace

    // Storage capability policy is separate from canonical-domain validity.
    static void validateCookedV1ModelSupport(const GtsModelAsset&         model,
                                             AssetCookResult&             result,
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

    static AssetCookResult planModelAsset(const GtsModelAsset&         model,
                                          const std::filesystem::path& sourcePath,
                                          const AssetCookerOptions&    options)
    {
        AssetCookResult result;
        const auto      validation = validateGtsModelAsset(model);
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
    static void publishCookedPackage(AssetCookResult& result, const std::filesystem::path& sourcePath)
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
        if (result.outputs.empty())
            return;
        const auto directory = result.outputs.front().path.parent_path();
        if (!ensureOutputDirectory(directory, result, sourcePath))
        {
            result.outputs.clear();
            return;
        }
        static std::atomic<uint64_t> sequence{0};
        auto                         staging =
            directory / (".assetc-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) +
                         "-" + std::to_string(sequence++));
        std::error_code error;
        if (!std::filesystem::create_directory(staging, error))
        {
            addCookDiagnostic(result,
                              AssetDiagnosticSeverity::Error,
                              "ASSET_COOK_WRITE_FAILED",
                              "Cannot stage package: " + error.message(),
                              sourcePath);
            result.outputs.clear();
            return;
        }
        std::vector<size_t> installed, backedUp;
        auto                fail = [&](const std::string& message)
        {
            addCookDiagnostic(result, AssetDiagnosticSeverity::Error, "ASSET_COOK_WRITE_FAILED", message, sourcePath);
            for (auto i : installed)
            {
                std::error_code ignored;
                std::filesystem::remove(result.outputs[i].path, ignored);
            }
            for (auto i : backedUp)
            {
                std::error_code restoreError;
                std::filesystem::rename(staging / (std::to_string(i) + ".old"), result.outputs[i].path, restoreError);
                if (restoreError)
                    addCookDiagnostic(result,
                                      AssetDiagnosticSeverity::Error,
                                      "ASSET_COOK_RESTORE_FAILED",
                                      "Previous output retained in " + staging.string() + ": " + restoreError.message(),
                                      sourcePath);
            }
            result.outputs.clear();
        };
        // Stage all writes before replacing any published component.
        for (size_t i = 0; i < bytes.size(); ++i)
        {
            std::ofstream file(staging / std::to_string(i), std::ios::binary | std::ios::trunc);
            file.write(reinterpret_cast<const char*>(bytes[i].data()), static_cast<std::streamsize>(bytes[i].size()));
            file.close();
            if (!file)
            {
                fail("Cannot stage " + result.outputs[i].path.string());
                break;
            }
        }
        for (size_t i = 0; i < result.outputs.size(); ++i)
        {
            const auto& path = result.outputs[i].path;
            if (std::filesystem::exists(path, error))
            {
                if (!std::filesystem::is_regular_file(path, error))
                {
                    fail("Output is not a regular file: " + path.string());
                    break;
                }
                std::filesystem::rename(path, staging / (std::to_string(i) + ".old"), error);
                if (error)
                {
                    fail("Cannot replace " + path.string() + ": " + error.message());
                    break;
                }
                backedUp.push_back(i);
            }
            std::filesystem::rename(staging / std::to_string(i), path, error);
            if (error)
            {
                fail("Cannot publish " + path.string() + ": " + error.message());
                break;
            }
            installed.push_back(i);
        }
        const bool restoreFailed = std::any_of(result.diagnostics.begin(),
                                               result.diagnostics.end(),
                                               [](const auto& d)
                                               {
                                                   return d.code == "ASSET_COOK_RESTORE_FAILED";
                                               });
        if (!restoreFailed)
            std::filesystem::remove_all(staging, error);
    }

    AssetCookResult AssetCooker::cookModelAsset(const GtsModelAsset&         model,
                                                const std::filesystem::path& sourcePath,
                                                const AssetCookerOptions&    options)
    {
        auto result = planModelAsset(model, sourcePath, options);
        publishCookedPackage(result, sourcePath);
        return result;
    }

    AssetCookResult AssetCooker::cookModelBundle(const GtsModelImportBundle&  bundle,
                                                 const std::filesystem::path& sourcePath,
                                                 const AssetCookerOptions&    options)
    {
        AssetCookResult result;
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

    AssetCookResult AssetCooker::cookSourceAsset(const std::filesystem::path& sourcePath,
                                                 const AssetCookerOptions&    options)
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
            const auto      imported = importer->importAsset({sourcePath});
            AssetCookResult result;
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

        AssetCookResult result;
        if (options.explicitImporter == "image" ||
            (options.explicitImporter.empty() && (extension == ".png" || extension == ".jpg" || extension == ".jpeg")))
        {
            const auto       outputDirectory = outputDirectoryFor(sourcePath, options);
            const auto       stem            = sanitizedName(sourcePath.stem().string(), "texture");
            TextureCookCache cache(outputDirectory, stem, options, result, sourcePath);
            auto             texture = textureInputForPath(sourcePath);
            cache.cookTexture(std::move(texture), options.textureRole, "", stem, sourcePath);
            if (result.textures.empty() && !result.hasErrors())
                addCookDiagnostic(result,
                                  AssetDiagnosticSeverity::Error,
                                  "TEXTURE_DECODE_FAILED",
                                  "Image cooking requires decoded pixels",
                                  sourcePath);
            publishCookedPackage(result, sourcePath);
            return result;
        }
        addCookDiagnostic(result,
                          AssetDiagnosticSeverity::Error,
                          "ASSET_IMPORTER_NOT_FOUND",
                          "No canonical model importer or image decoder for source: " + sourcePath.string(),
                          sourcePath);
        return result;
    }
} // namespace gts::rendering
