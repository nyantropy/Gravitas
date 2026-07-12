#pragma once

#include <algorithm>

#include "AssetTypes.h"
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

    inline MeshAssetData cookImportedMesh(const ImportedMesh& imported,
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
            asset.submeshes.push_back({
                primitive.firstIndex,
                primitive.indexCount,
                InvalidAssetId,
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
        asset.renderState = imported.renderState;
        asset.vertexColorOnly = imported.vertexColorOnly;
        return asset;
    }
}
