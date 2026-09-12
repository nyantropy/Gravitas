#include "AssetCooker.h"

#include <algorithm>
#include <cctype>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <system_error>
#include <utility>

#include "AssetSerializers.h"
#include "GltfAssetImporter.h"
#include "ImageAssetImporter.h"
#include "assets/importer/obj/GtsObjModelImporter.h"
#include "assets/model/GtsModelImportResult.h"
#include "assets/model/GtsModelAsset.h"
#include "assets/model/GtsModelValidation.h"
#include "assets/realization/GtsStaticModelRealization.h"
#include "TextureCooker.h"

namespace gts::rendering
{
namespace
{
    void addCookDiagnostic(AssetCookResult& result,
                           AssetDiagnosticSeverity severity,
                           std::string code,
                           std::string message,
                           const std::filesystem::path& sourcePath)
    {
        result.diagnostics.push_back({
            severity,
            std::move(code),
            std::move(message),
            sourcePath,
            0
        });
    }

    bool hasError(const std::vector<AssetDiagnostic>& diagnostics)
    {
        for (const AssetDiagnostic& diagnostic : diagnostics)
        {
            if (diagnostic.severity == AssetDiagnosticSeverity::Error)
                return true;
        }
        return false;
    }

    std::filesystem::path outputDirectoryFor(const std::filesystem::path& sourcePath,
                                             const AssetCookerOptions& options)
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

        const auto found = std::find_if(
            references.begin(),
            references.end(),
            [&reference](const AssetReference& existing)
            {
                return sameReference(existing, reference);
            });
        if (found == references.end())
            references.push_back(reference);
    }

    bool hasNonWhiteVertexColor(const MeshAssetData& mesh)
    {
        for (const Vertex& vertex : mesh.vertices)
        {
            if (vertex.color != glm::vec4(1.0f, 1.0f, 1.0f, 1.0f))
                return true;
        }
        return false;
    }

    bool hasUnassignedPrimitives(const std::vector<ImportedMesh>& meshes)
    {
        for (const ImportedMesh& mesh : meshes)
        {
            for (const ImportedMeshPrimitive& primitive : mesh.primitives)
            {
                if (primitive.materialIndex < 0)
                    return true;
            }
        }
        return false;
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

    std::filesystem::path materialTexturePathForRole(const ImportedMaterial& material,
                                                     MaterialTextureRole role)
    {
        switch (role)
        {
            case MaterialTextureRole::BaseColor:
                return material.baseColorTexture;
            case MaterialTextureRole::MetallicRoughness:
                return !material.metallicTexture.empty()
                    ? material.metallicTexture
                    : material.roughnessTexture;
            case MaterialTextureRole::Normal:
                return material.normalTexture;
            case MaterialTextureRole::AmbientOcclusion:
                return material.ambientOcclusionTexture;
            case MaterialTextureRole::Emissive:
                return material.emissiveTexture;
        }
        return {};
    }

    int32_t materialTextureIndexForRole(const ImportedMaterial& material,
                                        MaterialTextureRole role)
    {
        switch (role)
        {
            case MaterialTextureRole::BaseColor:
                return material.baseColorTextureIndex;
            case MaterialTextureRole::MetallicRoughness:
                return material.metallicRoughnessTextureIndex;
            case MaterialTextureRole::Normal:
                return material.normalTextureIndex;
            case MaterialTextureRole::AmbientOcclusion:
                return material.ambientOcclusionTextureIndex;
            case MaterialTextureRole::Emissive:
                return material.emissiveTextureIndex;
        }
        return -1;
    }

    std::string textureIdentityFor(const ImportedTexture& texture)
    {
        if (texture.source == ImportedTextureSource::ExternalFile && !texture.sourcePath.empty())
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

    std::string textureOutputStemFor(const ImportedTexture& texture,
                                     const std::string& fallback)
    {
        std::string stem;
        if (texture.source == ImportedTextureSource::ExternalFile && !texture.sourcePath.empty())
            stem = texture.sourcePath.stem().string();
        if (stem.empty() && !texture.logicalPath.empty())
            stem = std::filesystem::path(texture.logicalPath).stem().string();
        if (stem.empty())
            stem = texture.debugName;
        return sanitizedName(stem, fallback);
    }

    ImportedTexture importedTextureForPath(const std::filesystem::path& path,
                                           MaterialTextureRole role)
    {
        ImportedTexture texture;
        texture.debugName = path.stem().string();
        texture.sourcePath = path;
        texture.logicalPath = path.filename().generic_string();
        texture.colorSpace = colorSpaceForTextureCookRole(cookRoleForMaterialRole(role));
        texture.intendedRole = role;
        texture.source = ImportedTextureSource::ExternalFile;
        return texture;
    }

    bool sameNormalizedPath(const std::filesystem::path& lhs,
                            const std::filesystem::path& rhs)
    {
        return lhs.lexically_normal() == rhs.lexically_normal();
    }

    std::optional<ImportedTexture> importedTextureForMaterialRole(
        const AssetImportResult& importResult,
        const ImportedMaterial& material,
        MaterialTextureRole role)
    {
        const int32_t textureIndex = materialTextureIndexForRole(material, role);
        if (textureIndex >= 0 && static_cast<size_t>(textureIndex) < importResult.textures.size())
            return importResult.textures[static_cast<size_t>(textureIndex)];

        const std::filesystem::path texturePath = materialTexturePathForRole(material, role);
        if (texturePath.empty())
            return std::nullopt;

        const auto found = std::find_if(
            importResult.textures.begin(),
            importResult.textures.end(),
            [&texturePath](const ImportedTexture& texture)
            {
                return !texture.sourcePath.empty() && sameNormalizedPath(texture.sourcePath, texturePath);
            });
        if (found != importResult.textures.end())
            return *found;

        return importedTextureForPath(texturePath, role);
    }

    struct CookedMaterialTextureReferences
    {
        AssetReference baseColor;
        AssetReference metallicRoughness;
        AssetReference normal;
        AssetReference ambientOcclusion;
        AssetReference emissive;
    };

    void addMaterialTextureDependencies(MaterialAssetData& asset)
    {
        asset.dependencies.clear();
        addUniqueReference(asset.dependencies, asset.baseColorTexture);
        addUniqueReference(asset.dependencies, asset.metallicRoughnessTexture);
        addUniqueReference(asset.dependencies, asset.normalTexture);
        addUniqueReference(asset.dependencies, asset.ambientOcclusionTexture);
        addUniqueReference(asset.dependencies, asset.emissiveTexture);
    }

    MaterialAssetData makeMaterialAsset(const ImportedMaterial& imported,
                                        const AssetReference& cookedReference,
                                        const AssetCookerOptions& options,
                                        const CookedMaterialTextureReferences& textures)
    {
        MaterialAssetData asset = cookImportedMaterial(imported, cookedReference.id);
        asset.debugName = imported.name.empty()
            ? cookedReference.logicalPath
            : imported.name;
        if (options.vertexColorOnly)
            asset.vertexColorOnly = true;
        asset.shaderFamily = asset.vertexColorOnly
            ? MaterialShaderFamily::Unlit
            : MaterialShaderFamily::StandardSurface;

        asset.baseColorTexture = textures.baseColor;
        asset.metallicRoughnessTexture = textures.metallicRoughness;
        asset.normalTexture = textures.normal;
        asset.ambientOcclusionTexture = textures.ambientOcclusion;
        asset.emissiveTexture = textures.emissive;
        addMaterialTextureDependencies(asset);
        return asset;
    }

    ImportedMaterial makeDefaultImportedMaterial(const AssetCookerOptions& options,
                                                 bool vertexColorOnly)
    {
        ImportedMaterial material;
        material.name = vertexColorOnly ? "vertex_color" : "default";
        material.vertexColorOnly = vertexColorOnly;
        if (!options.baseColorTextureOverride.empty())
            material.baseColorTexture = options.baseColorTextureOverride;
        return material;
    }

    bool ensureOutputDirectory(const std::filesystem::path& outputDirectory,
                               AssetCookResult& result,
                               const std::filesystem::path& sourcePath)
    {
        std::error_code errorCode;
        std::filesystem::create_directories(outputDirectory, errorCode);
        if (errorCode)
        {
            addCookDiagnostic(
                result,
                AssetDiagnosticSeverity::Error,
                "ASSET_COOK_OUTPUT_DIRECTORY_FAILED",
                "Could not create asset output directory: " + outputDirectory.string(),
                sourcePath);
            return false;
        }
        return true;
    }

    bool writeMaterial(MaterialAssetData& material,
                       const std::filesystem::path& path,
                       AssetCookResult& result,
                       const std::filesystem::path& sourcePath)
    {
        std::string error;
        if (!MaterialAssetSerializer::writeFile(material, path, &error))
        {
            addCookDiagnostic(
                result,
                AssetDiagnosticSeverity::Error,
                "ASSET_COOK_MATERIAL_WRITE_FAILED",
                error,
                sourcePath);
            return false;
        }
        return true;
    }

    bool writeMesh(MeshAssetData& mesh,
                   const std::filesystem::path& path,
                   AssetCookResult& result,
                   const std::filesystem::path& sourcePath)
    {
        std::string error;
        if (!MeshAssetSerializer::writeFile(mesh, path, &error))
        {
            addCookDiagnostic(
                result,
                AssetDiagnosticSeverity::Error,
                "ASSET_COOK_MESH_WRITE_FAILED",
                error,
                sourcePath);
            return false;
        }
        return true;
    }

    bool writeModel(ModelAssetData& model,
                    const std::filesystem::path& path,
                    AssetCookResult& result,
                    const std::filesystem::path& sourcePath)
    {
        std::string error;
        if (!ModelAssetSerializer::writeFile(model, path, &error))
        {
            addCookDiagnostic(
                result,
                AssetDiagnosticSeverity::Error,
                "ASSET_COOK_MODEL_WRITE_FAILED",
                error,
                sourcePath);
            return false;
        }
        return true;
    }

    bool writeTexture(TextureAssetData& texture,
                      const std::filesystem::path& path,
                      AssetCookResult& result,
                      const std::filesystem::path& sourcePath)
    {
        std::string error;
        if (!TextureAssetSerializer::writeFile(texture, path, &error))
        {
            addCookDiagnostic(
                result,
                AssetDiagnosticSeverity::Error,
                "ASSET_COOK_TEXTURE_WRITE_FAILED",
                error,
                sourcePath);
            return false;
        }
        return true;
    }

    struct TextureCookCache
    {
        const std::filesystem::path& outputDirectory;
        const std::string& sourceStem;
        const AssetCookerOptions& options;
        AssetCookResult& result;
        const std::filesystem::path& sourcePath;

        std::map<std::string, AssetReference> referencesByKey;
        std::map<std::string, std::string> outputOwners;
        std::map<std::string, TextureCookRole> firstRoleByIdentity;
        std::set<std::string> reportedRoleConflicts;

        TextureCookCache(const std::filesystem::path& outputDirectory,
                         const std::string& sourceStem,
                         const AssetCookerOptions& options,
                         AssetCookResult& result,
                         const std::filesystem::path& sourcePath)
            : outputDirectory(outputDirectory)
            , sourceStem(sourceStem)
            , options(options)
            , result(result)
            , sourcePath(sourcePath)
        {
        }

        AssetReference cookTexture(ImportedTexture texture,
                                   TextureCookRole role,
                                   const std::string& outputSuffix,
                                   const std::string& fallbackName,
                                   const std::filesystem::path& diagnosticSource,
                                   const std::string& identityOverride = {})
        {
            const std::string identity = identityOverride.empty() ? textureIdentityFor(texture) : identityOverride;
            const auto roleFound = firstRoleByIdentity.find(identity);
            if (roleFound == firstRoleByIdentity.end())
            {
                firstRoleByIdentity.emplace(identity, role);
            }
            else if (roleFound->second != role && reportedRoleConflicts.insert(identity).second)
            {
                addCookDiagnostic(
                    result,
                    AssetDiagnosticSeverity::Warning,
                    "TEXTURE_COLOR_SPACE_CONFLICT",
                    "The same source texture is used with multiple texture roles; cooker emitted separate cooked variants",
                    diagnosticSource);
            }

            const std::string key =
                identity + "|role=" + std::to_string(static_cast<uint16_t>(role));
            const auto referenceFound = referencesByKey.find(key);
            if (referenceFound != referencesByKey.end())
                return referenceFound->second;

            if (!decodeTexture(texture, diagnosticSource))
                return {};

            const std::string baseStem =
                textureOutputStemFor(texture, sourceStem + "_" + fallbackName);
            std::string fileStem = baseStem + outputSuffix;
            std::string fileName = fileStem + ".gtex";
            uint32_t collisionIndex = 1;
            while (true)
            {
                const auto ownerFound = outputOwners.find(fileName);
                if (ownerFound == outputOwners.end() || ownerFound->second == key)
                    break;
                fileName = fileStem + "_" + std::to_string(collisionIndex++) + ".gtex";
            }
            outputOwners[fileName] = key;

            const std::filesystem::path outputPath = outputDirectory / fileName;
            const AssetReference reference = referenceForCookedOutput(outputPath);

            TextureCookerOptions textureOptions;
            textureOptions.role = role;
            textureOptions.generateMipmaps = options.generateTextureMipmaps;
            textureOptions.sampler = defaultSamplerForTextureCookRole(role, options.generateTextureMipmaps);
            textureOptions.debugName = baseStem;

            TextureCookResult cookResult =
                TextureCooker::cookImportedTexture(texture, reference.id, textureOptions, diagnosticSource);
            result.diagnostics.insert(
                result.diagnostics.end(),
                cookResult.diagnostics.begin(),
                cookResult.diagnostics.end());
            if (cookResult.hasErrors())
                return {};

            result.textures.push_back(std::move(cookResult.texture));
            if (!writeTexture(result.textures.back(), outputPath, result, sourcePath))
                return {};

            result.outputs.push_back({
                CookedAssetOutputType::Texture,
                outputPath,
                reference
            });
            referencesByKey.emplace(key, reference);
            return reference;
        }

    private:
        bool decodeTexture(ImportedTexture& texture,
                           const std::filesystem::path& diagnosticSource)
        {
            if (texture.decoded())
                return true;

            std::string error;
            ImportedTexture decoded = texture;
            if (texture.source == ImportedTextureSource::ExternalFile)
            {
                if (texture.sourcePath.empty())
                {
                    addCookDiagnostic(
                        result,
                        AssetDiagnosticSeverity::Warning,
                        "TEXTURE_SOURCE_MISSING",
                        "Material texture reference does not contain a source path",
                        diagnosticSource);
                    return false;
                }
                if (!std::filesystem::exists(texture.sourcePath))
                {
                    addCookDiagnostic(
                        result,
                        AssetDiagnosticSeverity::Warning,
                        "TEXTURE_SOURCE_MISSING",
                        "Texture source is missing: " + texture.sourcePath.string(),
                        diagnosticSource);
                    return false;
                }
                if (!ImageAssetImporter::decodeFile(texture.sourcePath, decoded, &error))
                {
                    addCookDiagnostic(
                        result,
                        AssetDiagnosticSeverity::Warning,
                        "TEXTURE_DECODE_FAILED",
                        error,
                        diagnosticSource);
                    return false;
                }
            }
            else
            {
                if (!ImageAssetImporter::decodeBytes(texture.embeddedBytes, decoded, &error))
                {
                    addCookDiagnostic(
                        result,
                        AssetDiagnosticSeverity::Warning,
                        "GLTF_EMBEDDED_IMAGE_INVALID",
                        error,
                        diagnosticSource);
                    return false;
                }
            }

            decoded.debugName = !texture.debugName.empty() ? texture.debugName : decoded.debugName;
            decoded.logicalPath = !texture.logicalPath.empty() ? texture.logicalPath : decoded.logicalPath;
            decoded.mimeType = !texture.mimeType.empty() ? texture.mimeType : decoded.mimeType;
            decoded.colorSpace = texture.colorSpace;
            decoded.intendedRole = texture.intendedRole;
            decoded.embeddedBytes = texture.embeddedBytes;
            texture = std::move(decoded);
            return true;
        }
    };

    void addModelDependencies(ModelAssetData& model)
    {
        for (const AssetReference& mesh : model.meshes)
            addUniqueReference(model.dependencies, mesh);
        for (const AssetReference& material : model.materials)
            addUniqueReference(model.dependencies, material);
    }
}

AssetCookResult AssetCooker::cookImportResult(const AssetImportResult& importResult,
                                              const std::filesystem::path& sourcePath,
                                              const AssetCookerOptions& options)
{
    AssetCookResult result;
    result.diagnostics = importResult.diagnostics;
    if (hasError(result.diagnostics))
        return result;

    const bool textureOnlyImport =
        importResult.meshes.empty() &&
        importResult.materials.empty() &&
        importResult.nodes.empty() &&
        !importResult.textures.empty();

    if (importResult.meshes.empty() && !textureOnlyImport)
    {
        addCookDiagnostic(
            result,
            AssetDiagnosticSeverity::Error,
            "ASSET_COOK_NO_MESHES",
            "Import result did not contain any meshes to cook",
            sourcePath);
        return result;
    }

    const std::filesystem::path outputDirectory = outputDirectoryFor(sourcePath, options);
    if (!ensureOutputDirectory(outputDirectory, result, sourcePath))
        return result;

    const std::string sourceStem = sanitizedName(sourcePath.stem().string(), "asset");

    TextureCookCache textureCache(
        outputDirectory,
        sourceStem,
        options,
        result,
        sourcePath);

    if (textureOnlyImport)
    {
        for (size_t textureIndex = 0; textureIndex < importResult.textures.size(); ++textureIndex)
        {
            const ImportedTexture& texture = importResult.textures[textureIndex];
            textureCache.cookTexture(
                texture,
                options.textureRole,
                "",
                "texture_" + std::to_string(textureIndex),
                texture.sourcePath.empty() ? sourcePath : texture.sourcePath);
        }
        return result;
    }

    auto cookMaterialTextures =
        [&](const ImportedMaterial& imported, const std::string& materialName)
        {
            CookedMaterialTextureReferences textures;
            for (MaterialTextureRole role : {
                     MaterialTextureRole::BaseColor,
                     MaterialTextureRole::MetallicRoughness,
                     MaterialTextureRole::Normal,
                     MaterialTextureRole::AmbientOcclusion,
                     MaterialTextureRole::Emissive})
            {
                std::optional<ImportedTexture> importedTexture =
                    importedTextureForMaterialRole(importResult, imported, role);
                if (!importedTexture.has_value())
                    continue;

                const std::filesystem::path diagnosticSource =
                    importedTexture->sourcePath.empty() ? sourcePath : importedTexture->sourcePath;
                AssetReference reference = textureCache.cookTexture(
                    std::move(*importedTexture),
                    cookRoleForMaterialRole(role),
                    materialRoleSuffix(role),
                    materialName + "_texture",
                    diagnosticSource);

                switch (role)
                {
                    case MaterialTextureRole::BaseColor:
                        textures.baseColor = reference;
                        break;
                    case MaterialTextureRole::MetallicRoughness:
                        textures.metallicRoughness = reference;
                        break;
                    case MaterialTextureRole::Normal:
                        textures.normal = reference;
                        break;
                    case MaterialTextureRole::AmbientOcclusion:
                        textures.ambientOcclusion = reference;
                        break;
                    case MaterialTextureRole::Emissive:
                        textures.emissive = reference;
                        break;
                }
            }
            return textures;
        };

    std::vector<AssetReference> materialReferences;
    materialReferences.reserve(importResult.materials.size());

    for (size_t materialIndex = 0; materialIndex < importResult.materials.size(); ++materialIndex)
    {
        const ImportedMaterial& imported = importResult.materials[materialIndex];
        const std::string materialName =
            sanitizedName(imported.name, "material_" + std::to_string(materialIndex));
        const std::filesystem::path materialPath =
            outputDirectory / (sourceStem + "_" + materialName + ".gmat");
        const AssetReference reference = referenceForCookedOutput(materialPath);
        CookedMaterialTextureReferences textureReferences =
            cookMaterialTextures(imported, materialName);
        MaterialAssetData material =
            makeMaterialAsset(imported, reference, options, textureReferences);

        if (material.baseColorTexture.empty())
        {
            addCookDiagnostic(
                result,
                AssetDiagnosticSeverity::Warning,
                "ASSET_COOK_DEFAULT_BASE_COLOR_TEXTURE",
                "Cooked material has no base color texture reference and will use runtime fallback texture",
                sourcePath);
        }

        result.materials.push_back(material);
        materialReferences.push_back(reference);
        if (writeMaterial(result.materials.back(), materialPath, result, sourcePath))
        {
            result.outputs.push_back({
                CookedAssetOutputType::Material,
                materialPath,
                reference
            });
        }
    }

    const bool needsUnassignedDefaultMaterial = hasUnassignedPrimitives(importResult.meshes);
    const bool needDefaultMaterial =
        materialReferences.empty() || needsUnassignedDefaultMaterial;
    AssetReference defaultMaterialReference;
    bool defaultMaterialCreated = false;
    if (needDefaultMaterial)
    {
        bool vertexColorOnly = options.vertexColorOnly;
        ImportedMaterial defaultImported = makeDefaultImportedMaterial(options, vertexColorOnly);
        if (!materialReferences.empty())
            defaultImported.name = vertexColorOnly ? "vertex_color_unassigned" : "default_unassigned";
        const std::filesystem::path materialPath =
            outputDirectory / (sourceStem + "_" + sanitizedName(defaultImported.name, "default") + ".gmat");
        defaultMaterialReference = referenceForCookedOutput(materialPath);

        CookedMaterialTextureReferences textureReferences =
            cookMaterialTextures(defaultImported, sanitizedName(defaultImported.name, "default"));
        MaterialAssetData material =
            makeMaterialAsset(defaultImported, defaultMaterialReference, options, textureReferences);
        material.shaderFamily = material.vertexColorOnly
            ? MaterialShaderFamily::Unlit
            : MaterialShaderFamily::StandardSurface;

        addCookDiagnostic(
            result,
            AssetDiagnosticSeverity::Warning,
            "ASSET_COOK_DEFAULT_MATERIAL",
            materialReferences.empty()
                ? "No source materials were imported; cooker emitted a default material"
                : "Some source primitives had no material assignment; cooker emitted a default material",
            sourcePath);

        result.materials.push_back(material);
        materialReferences.push_back(defaultMaterialReference);
        defaultMaterialCreated = true;
        if (writeMaterial(result.materials.back(), materialPath, result, sourcePath))
        {
            result.outputs.push_back({
                CookedAssetOutputType::Material,
                materialPath,
                defaultMaterialReference
            });
        }
    }

    std::vector<AssetReference> meshReferences;
    meshReferences.reserve(importResult.meshes.size());

    for (size_t meshIndex = 0; meshIndex < importResult.meshes.size(); ++meshIndex)
    {
        const ImportedMesh& imported = importResult.meshes[meshIndex];
        const std::string meshName = importResult.meshes.size() == 1
            ? sourceStem
            : sourceStem + "_" + sanitizedName(imported.debugName, "mesh_" + std::to_string(meshIndex));
        const std::filesystem::path meshPath = outputDirectory / (meshName + ".gmesh");
        const AssetReference meshReference = referenceForCookedOutput(meshPath);

        MeshAssetData mesh = cookImportedMesh(imported, materialReferences, meshReference.id);
        mesh.debugName = imported.debugName.empty() ? meshName : imported.debugName;

        if (defaultMaterialCreated)
        {
            for (SubmeshAssetData& submesh : mesh.submeshes)
            {
                if (submesh.material.empty())
                    submesh.material = defaultMaterialReference;
            }
            addUniqueReference(mesh.dependencies, defaultMaterialReference);
        }

        if (!options.vertexColorOnly && materialReferences.size() == 1 && hasNonWhiteVertexColor(mesh))
        {
            addCookDiagnostic(
                result,
                AssetDiagnosticSeverity::Warning,
                "ASSET_COOK_VERTEX_COLORS_PRESENT",
                "Cooked mesh contains vertex colors; material vertexColorOnly remains false unless explicitly requested",
                sourcePath);
        }

        if (mesh.generatedNormals)
        {
            addCookDiagnostic(
                result,
                AssetDiagnosticSeverity::Warning,
                "ASSET_COOK_GENERATED_NORMALS",
                "Cooked mesh generated missing normals during mesh preparation",
                sourcePath);
        }
        if (mesh.generatedTangents)
        {
            addCookDiagnostic(
                result,
                AssetDiagnosticSeverity::Warning,
                "ASSET_COOK_GENERATED_TANGENTS",
                "Cooked mesh generated missing tangents during mesh preparation",
                sourcePath);
        }

        result.meshes.push_back(std::move(mesh));
        if (writeMesh(result.meshes.back(), meshPath, result, sourcePath))
        {
            meshReferences.push_back(meshReference);
            result.outputs.push_back({
                CookedAssetOutputType::Mesh,
                meshPath,
                meshReference
            });
        }
    }

    if (!meshReferences.empty() && (!importResult.nodes.empty() || meshReferences.size() > 1u))
    {
        const std::filesystem::path modelPath = outputDirectory / (sourceStem + ".gmodel");
        const AssetReference modelReference = referenceForCookedOutput(modelPath);
        ModelAssetData model;
        model.id = modelReference.id;
        model.debugName = sourceStem;
        model.meshes = meshReferences;
        model.materials = materialReferences;

        if (!importResult.nodes.empty())
        {
            model.nodes.reserve(importResult.nodes.size());
            for (const ImportedNode& node : importResult.nodes)
            {
                ModelNodeAssetData cookedNode;
                cookedNode.name = node.name;
                cookedNode.parentIndex = node.parentIndex;
                cookedNode.localTransform = node.localTransform;
                if (node.meshIndex >= 0 && static_cast<size_t>(node.meshIndex) < meshReferences.size())
                    cookedNode.mesh = meshReferences[static_cast<size_t>(node.meshIndex)];
                model.nodes.push_back(std::move(cookedNode));
            }
        }
        else
        {
            for (size_t meshIndex = 0; meshIndex < meshReferences.size(); ++meshIndex)
            {
                ModelNodeAssetData node;
                node.name = sourceStem + "_node_" + std::to_string(meshIndex);
                node.mesh = meshReferences[meshIndex];
                model.nodes.push_back(std::move(node));
            }
        }

        addModelDependencies(model);
        result.models.push_back(std::move(model));
        if (writeModel(result.models.back(), modelPath, result, sourcePath))
        {
            result.outputs.push_back({
                CookedAssetOutputType::Model,
                modelPath,
                modelReference
            });
        }
    }

    return result;
}

AssetCookResult AssetCooker::cookModelAsset(const GtsModelAsset& model,
                                            const std::filesystem::path& sourcePath,
                                            const AssetCookerOptions& options)
{
    AssetCookResult result;
    std::vector<GtsModelDiagnostic> diagnostics;
    auto mesh = realizeGtsFlatStaticModel(model, diagnostics);
    for (const auto& diagnostic : diagnostics)
        addCookDiagnostic(result,
            diagnostic.severity == GtsModelDiagnosticSeverity::Error
                ? AssetDiagnosticSeverity::Error : AssetDiagnosticSeverity::Warning,
            diagnostic.code, diagnostic.location + ": " + diagnostic.message, sourcePath);
    if (!mesh)
        return result;

    const auto outputDirectory = outputDirectoryFor(sourcePath, options);
    const auto sourceStem = sanitizedName(sourcePath.stem().string(), "asset");
    // v1 has only UV0; reject image bindings that preparation cannot realize.
    auto checkBinding = [&](const GtsModelImageBinding& binding)
    {
        if (binding.texCoordSet != 0)
            addCookDiagnostic(result, AssetDiagnosticSeverity::Error, "ASSET_COOK_UV_SET_UNSUPPORTED",
                "Cooked v1 materials sample UV0 only; use UV0 or a future material storage profile", sourcePath);
    };
    for (const auto& material : model.materials)
    {
        for (const auto& binding : {material.baseColorImage, material.normalImage, material.emissiveImage})
            if (binding) checkBinding(*binding);
        for (const auto& binding : {material.metallicImage, material.roughnessImage, material.ambientOcclusionImage})
            if (binding) checkBinding(binding->image);
    }
    if (result.hasErrors() || !ensureOutputDirectory(outputDirectory, result, sourcePath))
        return result;

    TextureCookCache cache(outputDirectory, sourceStem, options, result, sourcePath);
    // Reuse the existing image decoder/mip machinery, never legacy model DTOs.
    auto imageInput = [&](uint32_t index)
    {
        const auto& image = model.images[index];
        ImportedTexture texture;
        texture.debugName = image.name;
        if (const auto* path = std::get_if<std::filesystem::path>(&image.source))
        {
            texture.source = ImportedTextureSource::ExternalFile;
            texture.sourcePath = *path;
        }
        else
        {
            const auto& embedded = std::get<GtsModelEmbeddedImage>(image.source);
            texture.source = ImportedTextureSource::EmbeddedBytes;
            texture.embeddedBytes = embedded.bytes;
            texture.mimeType = embedded.mimeType;
        }
        return texture;
    };
    auto cookImage = [&](const std::optional<GtsModelImageBinding>& binding,
                         MaterialTextureRole role, const std::string& name)
    {
        if (!binding) return AssetReference{};
        return cache.cookTexture(imageInput(binding->imageIndex), cookRoleForMaterialRole(role),
                                 materialRoleSuffix(role), name, sourcePath);
    };
    auto decode = [&](const GtsModelScalarImageBinding& binding) -> std::optional<ImportedTexture>
    {
        auto texture = imageInput(binding.image.imageIndex);
        std::string error;
        const bool loaded = texture.source == ImportedTextureSource::ExternalFile
            ? ImageAssetImporter::decodeFile(texture.sourcePath, texture, &error)
            : ImageAssetImporter::decodeBytes(texture.embeddedBytes, texture, &error);
        if (!loaded)
        {
            addCookDiagnostic(result, AssetDiagnosticSeverity::Error, "ASSET_COOK_SCALAR_IMAGE_FAILED",
                "Cannot decode scalar image: " + error, sourcePath);
            return std::nullopt;
        }
        return texture;
    };
    auto scalarKey = [](const std::optional<GtsModelScalarImageBinding>& binding)
    {
        return binding ? std::to_string(binding->image.imageIndex) + "_" +
            std::to_string(static_cast<int>(binding->channel)) : "none";
    };
    auto cookScalars = [&](const std::optional<GtsModelScalarImageBinding>& first,
                           const std::optional<GtsModelScalarImageBinding>& second,
                           bool metallicRoughness, const std::string& name)
    {
        if (!first && !second) return AssetReference{};
        const auto a = first ? decode(*first) : std::nullopt;
        const auto b = second ? decode(*second) : std::nullopt;
        if ((first && !a) || (second && !b)) return AssetReference{};
        if (a && b && (a->width != b->width || a->height != b->height))
        {
            addCookDiagnostic(result, AssetDiagnosticSeverity::Error, "ASSET_COOK_SCALAR_IMAGE_SIZE",
                "Metallic and roughness images must have equal dimensions for v1 packing; resample them before cooking", sourcePath);
            return AssetReference{};
        }
        ImportedTexture packed;
        packed.width = a ? a->width : b->width;
        packed.height = a ? a->height : b->height;
        packed.sourceChannelCount = 4;
        packed.debugName = sourceStem + "_" + name;
        packed.logicalPath = packed.debugName;
        packed.rgba8Pixels.assign(static_cast<size_t>(packed.width) * packed.height * 4, 255);
        for (size_t pixel = 0; pixel < packed.rgba8Pixels.size(); pixel += 4)
        {
            if (a) packed.rgba8Pixels[pixel + (metallicRoughness ? 2 : 0)] =
                a->rgba8Pixels[pixel + static_cast<size_t>(first->channel)];
            if (b) packed.rgba8Pixels[pixel + 1] = b->rgba8Pixels[pixel + static_cast<size_t>(second->channel)];
        }
        const auto role = metallicRoughness ? TextureCookRole::MetallicRoughness : TextureCookRole::AmbientOcclusion;
        const auto suffix = metallicRoughness ? "_metallic_roughness" : "_ao";
        return cache.cookTexture(std::move(packed), role, suffix, name, sourcePath,
                                 "scalar:" + scalarKey(first) + ":" + scalarKey(second));
    };

    std::vector<AssetReference> materialReferences;
    std::set<std::string> materialNames;
    auto cookMaterial = [&](const GtsModelMaterial& imported, bool isDefault)
    {
        std::string name = sanitizedName(imported.name, "material_" + std::to_string(materialReferences.size()));
        const auto baseName = name;
        for (uint32_t i = 1; !materialNames.insert(name).second; ++i)
            name = baseName + "_" + std::to_string(i);
        const auto path = outputDirectory / (sourceStem + "_" + name + ".gmat");
        const auto reference = referenceForCookedOutput(path);
        MaterialAssetData material;
        material.id = reference.id;
        material.debugName = imported.name;
        material.baseColor = imported.baseColor;
        material.metallic = imported.metallic;
        material.roughness = imported.roughness;
        material.normalScale = imported.normalScale;
        material.ambientOcclusionStrength = imported.ambientOcclusionStrength;
        material.emissiveFactor = imported.emissiveFactor;
        material.emissiveStrength = imported.emissiveStrength;
        switch (imported.alphaMode)
        {
            case GtsModelAlphaMode::Opaque: material.renderState.alphaMode = MaterialAlphaMode::Opaque; break;
            case GtsModelAlphaMode::Mask: material.renderState.alphaMode = MaterialAlphaMode::Mask; break;
            case GtsModelAlphaMode::Blend: material.renderState.alphaMode = MaterialAlphaMode::Blend; break;
        }
        material.renderState.alphaCutoff = imported.alphaCutoff;
        material.renderState.doubleSided = imported.doubleSided;
        material.renderState.depthWrite = imported.alphaMode != GtsModelAlphaMode::Blend;
        material.vertexColorOnly = options.vertexColorOnly;
        material.shaderFamily = options.vertexColorOnly ? MaterialShaderFamily::Unlit : MaterialShaderFamily::StandardSurface;
        material.baseColorTexture = cookImage(imported.baseColorImage, MaterialTextureRole::BaseColor, name);
        if (isDefault && !options.baseColorTextureOverride.empty())
            material.baseColorTexture = cache.cookTexture(
                importedTextureForPath(options.baseColorTextureOverride, MaterialTextureRole::BaseColor),
                TextureCookRole::BaseColor, "", name, sourcePath);
        material.normalTexture = cookImage(imported.normalImage, MaterialTextureRole::Normal, name);
        material.emissiveTexture = cookImage(imported.emissiveImage, MaterialTextureRole::Emissive, name);
        material.metallicRoughnessTexture = cookScalars(imported.metallicImage, imported.roughnessImage, true, name);
        material.ambientOcclusionTexture = cookScalars(imported.ambientOcclusionImage, std::nullopt, false, name);
        addMaterialTextureDependencies(material);
        if (material.baseColorTexture.empty())
            addCookDiagnostic(result, AssetDiagnosticSeverity::Warning, "ASSET_COOK_DEFAULT_BASE_COLOR_TEXTURE",
                "Cooked material uses the runtime fallback base color texture", sourcePath);
        result.materials.push_back(std::move(material));
        materialReferences.push_back(reference);
        if (!result.hasErrors() && writeMaterial(result.materials.back(), path, result, sourcePath))
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
        if (!model.materials.empty()) material.name += "_unassigned";
        defaultMaterial = cookMaterial(material, true);
        addCookDiagnostic(result, AssetDiagnosticSeverity::Warning, "ASSET_COOK_DEFAULT_MATERIAL",
            "Cooker emitted a default material for unassigned primitives", sourcePath);
    }
    if (result.hasErrors()) return result;

    size_t primitiveIndex = 0;
    for (const auto& sourceMesh : model.meshes)
        for (const auto& primitive : sourceMesh.primitives)
        {
            const auto reference = primitive.materialIndex ? materialReferences[*primitive.materialIndex] : defaultMaterial;
            mesh->submeshes[primitiveIndex++].material = reference;
            addUniqueReference(mesh->dependencies, reference);
        }
    const auto path = outputDirectory / (sourceStem + ".gmesh");
    const auto reference = referenceForCookedOutput(path);
    mesh->id = reference.id;
    mesh->debugName = sourceStem;
    if (mesh->generatedNormals)
        addCookDiagnostic(result, AssetDiagnosticSeverity::Warning, "ASSET_COOK_GENERATED_NORMALS",
            "Static preparation generated missing normals", sourcePath);
    if (mesh->generatedTangents)
        addCookDiagnostic(result, AssetDiagnosticSeverity::Warning, "ASSET_COOK_GENERATED_TANGENTS",
            "Static preparation generated missing tangents", sourcePath);
    if (!options.vertexColorOnly && hasNonWhiteVertexColor(*mesh))
        addCookDiagnostic(result, AssetDiagnosticSeverity::Warning, "ASSET_COOK_VERTEX_COLORS_PRESENT",
            "Cooked mesh contains vertex colors; vertexColorOnly remains an explicit cooking option", sourcePath);
    result.meshes.push_back(std::move(*mesh));
    if (writeMesh(result.meshes.back(), path, result, sourcePath))
        result.outputs.push_back({CookedAssetOutputType::Mesh, path, reference});
    return result;
}

AssetCookResult AssetCooker::cookSourceAsset(const std::filesystem::path& sourcePath,
                                             const AssetCookerOptions& options)
{
    std::string extension = sourcePath.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    if (options.explicitImporter == "obj" || (options.explicitImporter.empty() && extension == ".obj"))
    {
        const auto imported = GtsObjModelImporter{}.importAsset({sourcePath});
        AssetCookResult result;
        if (imported.succeeded())
            result = cookModelAsset(*imported.asset(), sourcePath, options);
        for (const auto& diagnostic : imported.diagnostics())
            addCookDiagnostic(result,
                diagnostic.severity == GtsModelDiagnosticSeverity::Error
                    ? AssetDiagnosticSeverity::Error : AssetDiagnosticSeverity::Warning,
                diagnostic.code, diagnostic.location + ": " + diagnostic.message, sourcePath);
        return result;
    }

    AssetImporterRegistry registry;
    registry.registerImporter(std::make_unique<GltfAssetImporter>());
    registry.registerImporter(std::make_unique<ImageAssetImporter>());

    IAssetImporter* importer = registry.findImporter(sourcePath, options.explicitImporter);
    if (importer == nullptr)
    {
        AssetImportResult missingImporter;
        missingImporter.diagnostics.push_back({
            AssetDiagnosticSeverity::Error,
            "ASSET_IMPORTER_NOT_FOUND",
            "No importer is registered for source asset: " + sourcePath.string(),
            sourcePath,
            0
        });
        return cookImportResult(missingImporter, sourcePath, options);
    }

    AssetImportRequest request;
    request.sourcePath = sourcePath;
    request.explicitImporter = options.explicitImporter;
    request.requestedCapabilities = importer->capabilities() & AssetImportCapability::All;

    AssetImportResult importResult = importer->importAsset(request);
    return cookImportResult(importResult, sourcePath, options);
}
}
