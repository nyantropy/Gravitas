#pragma once

#include <vector>
#include "Vertex.h"

struct GtsModelPrimitive;

namespace gtsGeometryPreparationDetail
{
    struct PrimitiveGeometry
    {
        std::vector<Vertex>   vertices;
        std::vector<uint32_t> indices;
        MeshGeometryMetadata  metadata;
    };

    // Input must already pass primitive/profile validation. Skin streams are handled by the caller.
    PrimitiveGeometry preparePrimitiveGeometry(const GtsModelPrimitive& primitive);
} // namespace gtsGeometryPreparationDetail
