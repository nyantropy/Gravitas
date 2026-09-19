#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "assets/geometry/GtsStaticVertex.h"
#include "assets/geometry/GtsGeometryMetadata.h"

struct GtsPreparedStaticPrimitive
{
    uint32_t                firstIndex = 0;
    uint32_t                indexCount = 0;
    std::optional<uint32_t> materialIndex;
    MeshGeometryMetadata    metadata;
};

// current static rendering profile, not canonical imported geometry or GPU state
struct GtsPreparedStaticMesh
{
    std::string                             name;
    std::vector<GtsStaticVertex>                     vertices;
    std::vector<uint32_t>                   indices;
    std::vector<GtsPreparedStaticPrimitive> primitives;

    // attribute intersection across primitives; generated flags are any-of
    // counts describe the combined buffers
    MeshGeometryMetadata metadata;
};
