#include "AssetCooker.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <memory>
#include <system_error>
#include <utility>

#include "AssetSerializers.h"
#include "GltfAssetImporter.h"
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

    AssetReference referenceForImportedTexture(int32_t textureIndex,
                                               const std::filesystem::path& fallbackPath,
                                               const std::vector<ImportedTexture>& textures,
                                               const std::filesystem::path& sourceDirectory)
    {
        if (textureIndex >= 0 && static_cast<size_t>(textureIndex) < textures.size())
        {
            const ImportedTexture& texture = textures[static_cast<size_t>(textureIndex)];
            if (!texture.logicalPath.empty())
                return AssetReference::fromLogicalPath(texture.logicalPath);
            if (!texture.sourcePath.empty())
                return referenceForSourceDependency(texture.sourcePath, sourceDirectory);
        }
        return referenceForSourceDependency(fallbackPath, sourceDirectory);
    }

    MaterialAssetData makeMaterialAsset(const ImportedMaterial& imported,
                                        const AssetReference& cookedReference,
                                        const AssetCookerOptions& options,
                                        const std::filesystem::path& sourceDirectory,
                                        const std::vector<ImportedTexture>& textures)
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
            asset.baseColorTexture = referenceForImportedTexture(
                imported.baseColorTextureIndex,
                imported.baseColorTexture,
                textures,
                sourceDirectory);

        asset.metallicRoughnessTexture = referenceForImportedTexture(
            imported.metallicRoughnessTextureIndex,
            !imported.metallicTexture.empty() ? imported.metallicTexture : imported.roughnessTexture,
            textures,
            sourceDirectory);
        asset.normalTexture = referenceForImportedTexture(
            imported.normalTextureIndex,
            imported.normalTexture,
            textures,
            sourceDirectory);
        asset.ambientOcclusionTexture = referenceForImportedTexture(
            imported.ambientOcclusionTextureIndex,
            imported.ambientOcclusionTexture,
            textures,
            sourceDirectory);
        asset.emissiveTexture = referenceForImportedTexture(
            imported.emissiveTextureIndex,
            imported.emissiveTexture,
            textures,
            sourceDirectory);

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

    void writeEmbeddedTextures(const std::vector<ImportedTexture>& textures,
                               const std::filesystem::path& outputDirectory,
                               AssetCookResult& result,
                               const std::filesystem::path& sourcePath)
    {
        for (const ImportedTexture& texture : textures)
        {
            if (!texture.embedded())
                continue;

            const std::filesystem::path outputPath =
                outputDirectory / std::filesystem::path(
                    texture.logicalPath.empty()
                        ? texture.debugName
                        : texture.logicalPath);
            if (!outputPath.parent_path().empty())
                std::filesystem::create_directories(outputPath.parent_path());

            std::ofstream file(outputPath, std::ios::binary | std::ios::trunc);
            if (!file)
            {
                addCookDiagnostic(
                    result,
                    AssetDiagnosticSeverity::Error,
                    "ASSET_COOK_EMBEDDED_TEXTURE_WRITE_FAILED",
                    "Could not write embedded texture dependency: " + outputPath.string(),
                    sourcePath);
                continue;
            }
            file.write(
                reinterpret_cast<const char*>(texture.embeddedBytes.data()),
                static_cast<std::streamsize>(texture.embeddedBytes.size()));
            if (!file.good())
            {
                addCookDiagnostic(
                    result,
                    AssetDiagnosticSeverity::Error,
                    "ASSET_COOK_EMBEDDED_TEXTURE_WRITE_FAILED",
                    "Could not write embedded texture dependency: " + outputPath.string(),
                    sourcePath);
                continue;
            }

            result.outputs.push_back({
                CookedAssetOutputType::TextureDependency,
                outputPath,
                AssetReference::fromLogicalPath(texture.logicalPath)
            });
        }
    }

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

    writeEmbeddedTextures(importResult.textures, outputDirectory, result, sourcePath);

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
            makeMaterialAsset(imported, reference, options, sourceDirectory, importResult.textures);

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

        MaterialAssetData material =
            makeMaterialAsset(defaultImported, defaultMaterialReference, options, sourceDirectory, importResult.textures);
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

AssetCookResult AssetCooker::cookSourceAsset(const std::filesystem::path& sourcePath,
                                             const AssetCookerOptions& options)
{
    AssetImporterRegistry registry;
    registry.registerImporter(std::make_unique<ObjAssetImporter>());
    registry.registerImporter(std::make_unique<GltfAssetImporter>());

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
