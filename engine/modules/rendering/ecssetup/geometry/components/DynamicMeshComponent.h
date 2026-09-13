#pragma once

#include <cstdint>
#include <vector>

#include "assets/processing/geometry/static/GtsStaticVertex.h"
#include "assets/processing/geometry/GtsGeometryMetadata.h"

// gameplay facing description of runtime-generated geometry
// the renderer uploads the components own vertices/indices
// best used for custom geometry that is unique or changes over time
struct DynamicMeshComponent
{
    std::vector<GtsStaticVertex>   vertices;
    std::vector<uint32_t> indices;
    uint64_t              geometryVersion = 0;
    VertexAttributeFlags  sourceAttributes = UnlitVertexAttributes;
};
