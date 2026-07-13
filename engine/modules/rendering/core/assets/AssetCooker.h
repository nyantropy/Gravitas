#pragma once

#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

#include "AssetTypes.h"
#include "AssetImporter.h"
#include "MeshGeometryProcessor.h"

namespace gts::rendering
{
    inline AssetBounds computeAssetBounds(const std::vector<Vertex>& vertices)
    {
        AssetBounds bounds;
        if (vertices.empty())
            return bounds;

        bounds.min = vertices.front().pos;
        bounds.max = vertices.front().pos;
        bounds.valid = true;

        for (const Vertex& vertex : vertices)
        {
            bounds.min = glm::min(bounds.min, vertex.pos);
            bounds.max = glm::max(bounds.max, vertex.pos);
        }

        return bounds;
    }

    inline MeshGeometryMetadata meshMetadataFromAssetData(const MeshAssetData& asset)
    {
        return MeshGeometryMetadata{
            asset.attributes,
            static_cast<uint32_t>(asset.vertices.size()),
            static_cast<uint32_t>(asset.indices.size()),
            asset.generatedNormals,
            asset.generatedTangents
        };
    }

    inline std::string logicalPathString(const std::filesystem::path& path)
    {
        return path.lexically_normal().generic_string();
    }

    inline AssetReference referenceFromPath(const std::filesystem::path& path)
    {
        if (path.empty())
            return {};
        return AssetReference::fromLogicalPath(logicalPathString(path));
    }

    inline MeshAssetData cookImportedMesh(const ImportedMesh& imported,
                                          const std::vector<AssetReference>& materialReferences = {},
                                          AssetId assetId = InvalidAssetId)
    {
        MeshAssetData asset;
        asset.id = assetId;
        asset.debugName = imported.debugName;
        asset.vertices = imported.vertices;
        asset.indices = imported.indices;

        asset.submeshes.reserve(imported.primitives.size());
        for (const ImportedMeshPrimitive& primitive : imported.primitives)
        {
            AssetReference material;
            if (primitive.materialIndex >= 0 &&
                static_cast<size_t>(primitive.materialIndex) < materialReferences.size())
            {
                material = materialReferences[static_cast<size_t>(primitive.materialIndex)];
                asset.dependencies.push_back(material);
            }

            asset.submeshes.push_back({
                primitive.firstIndex,
                primitive.indexCount,
                material,
                primitive.name
            });
        }

        MeshGeometryMetadata metadata = prepareMeshGeometry(
            asset.vertices,
            asset.indices,
            imported.sourceAttributes);
        asset.attributes = metadata.attributes;
        asset.generatedNormals = metadata.generatedNormals;
        asset.generatedTangents = metadata.generatedTangents;
        asset.bounds = computeAssetBounds(asset.vertices);
        return asset;
    }

    inline MaterialAssetData cookImportedMaterial(const ImportedMaterial& imported,
                                                  AssetId assetId = InvalidAssetId)
    {
        MaterialAssetData asset;
        asset.id = assetId;
        asset.debugName = imported.name;
        asset.baseColor = imported.baseColor.value_or(asset.baseColor);
        asset.metallic = imported.metallic.value_or(asset.metallic);
        asset.roughness = imported.roughness.value_or(asset.roughness);
        asset.normalScale = imported.normalScale.value_or(asset.normalScale);
        asset.ambientOcclusionStrength =
            imported.ambientOcclusionStrength.value_or(asset.ambientOcclusionStrength);
        asset.emissiveFactor = imported.emissiveFactor.value_or(asset.emissiveFactor);
        asset.emissiveStrength = imported.emissiveStrength.value_or(asset.emissiveStrength);
        asset.baseColorTexture = referenceFromPath(imported.baseColorTexture);
        asset.metallicRoughnessTexture = referenceFromPath(
            !imported.metallicTexture.empty() ? imported.metallicTexture : imported.roughnessTexture);
        asset.normalTexture = referenceFromPath(imported.normalTexture);
        asset.ambientOcclusionTexture = referenceFromPath(imported.ambientOcclusionTexture);
        asset.emissiveTexture = referenceFromPath(imported.emissiveTexture);
        for (const AssetReference& reference : {
                 asset.baseColorTexture,
                 asset.metallicRoughnessTexture,
                 asset.normalTexture,
                 asset.ambientOcclusionTexture,
                 asset.emissiveTexture})
        {
            if (!reference.empty())
                asset.dependencies.push_back(reference);
        }
        asset.renderState = imported.renderState;
        asset.vertexColorOnly = imported.vertexColorOnly;
        return asset;
    }

    enum class CookedAssetOutputType
    {
        Mesh,
        Material,
        Model,
        TextureDependency
    };

    struct CookedAssetOutput
    {
        CookedAssetOutputType type = CookedAssetOutputType::Mesh;
        std::filesystem::path path;
        AssetReference reference;
    };

    struct AssetCookerOptions
    {
        std::filesystem::path outputDirectory;
        std::filesystem::path baseColorTextureOverride;
        bool vertexColorOnly = false;
        std::string explicitImporter;
    };

    struct AssetCookResult
    {
        std::vector<MeshAssetData> meshes;
        std::vector<MaterialAssetData> materials;
        std::vector<ModelAssetData> models;
        std::vector<CookedAssetOutput> outputs;
        std::vector<AssetDiagnostic> diagnostics;

        bool hasErrors() const
        {
            for (const AssetDiagnostic& diagnostic : diagnostics)
            {
                if (diagnostic.severity == AssetDiagnosticSeverity::Error)
                    return true;
            }
            return false;
        }

        bool succeeded() const
        {
            return !hasErrors();
        }
    };

    class AssetCooker
    {
    public:
        static AssetCookResult cookImportResult(const AssetImportResult& importResult,
                                                const std::filesystem::path& sourcePath,
                                                const AssetCookerOptions& options);

        static AssetCookResult cookSourceAsset(const std::filesystem::path& sourcePath,
                                               const AssetCookerOptions& options);
    };
}
