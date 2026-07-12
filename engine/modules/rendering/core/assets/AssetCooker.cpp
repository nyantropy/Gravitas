#include "AssetCooker.h"

#include <algorithm>
#include <cctype>
#include <memory>
#include <system_error>
#include <utility>

#include "AssetSerializers.h"
#include "ObjAssetImporter.h"

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

    bool startsWithParentTraversal(const std::filesystem::path& path)
    {
        auto it = path.begin();
        return it != path.end() && *it == "..";
    }

    AssetReference referenceForSourceDependency(const std::filesystem::path& path,
                                                const std::filesystem::path& sourceDirectory)
    {
        if (path.empty())
            return {};

        const std::filesystem::path normalizedPath = path.lexically_normal();
        const std::filesystem::path normalizedBase = sourceDirectory.lexically_normal();
        const std::filesystem::path relative = normalizedPath.lexically_relative(normalizedBase);
        if (!relative.empty() && !startsWithParentTraversal(relative))
            return AssetReference::fromLogicalPath(relative.generic_string());

        return AssetReference::fromLogicalPath(normalizedPath.generic_string());
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

    MaterialAssetData makeMaterialAsset(const ImportedMaterial& imported,
                                        const AssetReference& cookedReference,
                                        const AssetCookerOptions& options,
                                        const std::filesystem::path& sourceDirectory)
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

        if (!options.baseColorTextureOverride.empty())
            asset.baseColorTexture =
                referenceForSourceDependency(options.baseColorTextureOverride, sourceDirectory);
        else
            asset.baseColorTexture =
                referenceForSourceDependency(imported.baseColorTexture, sourceDirectory);

        asset.metallicRoughnessTexture = referenceForSourceDependency(
            !imported.metallicTexture.empty() ? imported.metallicTexture : imported.roughnessTexture,
            sourceDirectory);
        asset.normalTexture = referenceForSourceDependency(imported.normalTexture, sourceDirectory);
        asset.ambientOcclusionTexture =
            referenceForSourceDependency(imported.ambientOcclusionTexture, sourceDirectory);
        asset.emissiveTexture = referenceForSourceDependency(imported.emissiveTexture, sourceDirectory);

        asset.dependencies.clear();
        addUniqueReference(asset.dependencies, asset.baseColorTexture);
        addUniqueReference(asset.dependencies, asset.metallicRoughnessTexture);
        addUniqueReference(asset.dependencies, asset.normalTexture);
        addUniqueReference(asset.dependencies, asset.ambientOcclusionTexture);
        addUniqueReference(asset.dependencies, asset.emissiveTexture);
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
}

AssetCookResult AssetCooker::cookImportResult(const AssetImportResult& importResult,
                                              const std::filesystem::path& sourcePath,
                                              const AssetCookerOptions& options)
{
    AssetCookResult result;
    result.diagnostics = importResult.diagnostics;
    if (hasError(result.diagnostics))
        return result;

    if (importResult.meshes.empty())
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

    const std::filesystem::path sourceDirectory = sourcePath.parent_path();
    const std::string sourceStem = sanitizedName(sourcePath.stem().string(), "asset");

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
        MaterialAssetData material =
            makeMaterialAsset(imported, reference, options, sourceDirectory);

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

    const bool needDefaultMaterial =
        materialReferences.empty() || !options.baseColorTextureOverride.empty() || options.vertexColorOnly;
    AssetReference defaultMaterialReference;
    bool defaultMaterialCreated = false;
    if (needDefaultMaterial && materialReferences.empty())
    {
        bool vertexColorOnly = options.vertexColorOnly;
        ImportedMaterial defaultImported = makeDefaultImportedMaterial(options, vertexColorOnly);
        const std::filesystem::path materialPath =
            outputDirectory / (sourceStem + "_" + sanitizedName(defaultImported.name, "default") + ".gmat");
        defaultMaterialReference = referenceForCookedOutput(materialPath);

        MaterialAssetData material =
            makeMaterialAsset(defaultImported, defaultMaterialReference, options, sourceDirectory);
        material.shaderFamily = material.vertexColorOnly
            ? MaterialShaderFamily::Unlit
            : MaterialShaderFamily::StandardSurface;

        addCookDiagnostic(
            result,
            AssetDiagnosticSeverity::Warning,
            "ASSET_COOK_DEFAULT_MATERIAL",
            "No source materials were imported; cooker emitted a default material",
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

        if (defaultMaterialCreated && materialReferences.size() == 1)
        {
            for (SubmeshAssetData& submesh : mesh.submeshes)
            {
                if (submesh.material.empty())
                    submesh.material = materialReferences.front();
            }
            addUniqueReference(mesh.dependencies, materialReferences.front());
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
            result.outputs.push_back({
                CookedAssetOutputType::Mesh,
                meshPath,
                meshReference
            });
        }
    }

    return result;
}

AssetCookResult AssetCooker::cookSourceAsset(const std::filesystem::path& sourcePath,
                                             const AssetCookerOptions& options)
{
    AssetImporterRegistry registry;
    registry.registerImporter(std::make_unique<ObjAssetImporter>());

    AssetImportRequest request;
    request.sourcePath = sourcePath;
    request.explicitImporter = options.explicitImporter;
    request.requestedCapabilities = AssetImportCapability::Meshes
        | AssetImportCapability::Materials
        | AssetImportCapability::Textures;

    AssetImportResult importResult = registry.importAsset(request);
    return cookImportResult(importResult, sourcePath, options);
}
}
