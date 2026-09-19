#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "GtsSkinnedVertex.h"
#include "assets/geometry/GtsGeometryMetadata.h"

struct GtsSkinnedInfluenceMetadata
{
    uint32_t reducedInfluenceVertices = 0;
    size_t   maxSourceInfluenceCount  = 0; // nonzero entries across all numbered sets
};

struct GtsPreparedSkinnedPrimitive
{
    uint32_t                    firstIndex = 0;
    uint32_t                    indexCount = 0;
    std::optional<uint32_t>     materialIndex;
    MeshGeometryMetadata        metadata;
    GtsSkinnedInfluenceMetadata influences;
};

struct GtsPreparedSkinnedMesh
{
    std::string                              name;
    std::vector<GtsSkinnedVertex>            vertices;
    std::vector<uint32_t>                    indices;
    std::vector<GtsPreparedSkinnedPrimitive> primitives;
    // geometry flags intersect, generation flags OR; influence counts sum/max respectively
    MeshGeometryMetadata        metadata;
    GtsSkinnedInfluenceMetadata influences;
};
