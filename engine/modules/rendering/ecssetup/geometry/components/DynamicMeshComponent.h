#pragma once

#include <cstdint>
#include <vector>

#include "Vertex.h"

// gameplay facing description of runtime-generated geometry
// the renderer uploads the components own vertices/indices
// best used for custom geometry that is unique or changes over time
struct DynamicMeshComponent
{
    std::vector<Vertex>   vertices;
    std::vector<uint32_t> indices;
    uint64_t              geometryVersion = 0;
};
