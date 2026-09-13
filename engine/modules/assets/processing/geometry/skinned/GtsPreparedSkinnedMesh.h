#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "Vertex.h"

// CPU profile only; no GPU packing/alignment contract is established here
struct GtsSkinnedVertex
{
    glm::vec3  pos{0};
    glm::vec3  normal{0, 0, 1};
    glm::vec4  tangent{1, 0, 0, 1};
    glm::vec4  color{1};
    glm::vec2  texCoord{0};
    glm::uvec4 joints{0}; // skin-local slots, never skeleton-node indices
    glm::vec4  weights{0};
};

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
