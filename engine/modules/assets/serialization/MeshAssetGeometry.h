#pragma once

#include "assets/serialization/AssetTypes.h"

namespace gts::rendering
{
    inline AssetBounds computeAssetBounds(const std::vector<GtsStaticVertex>& vertices)
    {
        AssetBounds bounds;
        if (vertices.empty())
            return bounds;

        bounds.min = vertices.front().pos;
        bounds.max = vertices.front().pos;
        bounds.valid = true;

        for (const GtsStaticVertex& vertex : vertices)
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

}
