#pragma once

#include <vector>
#include "assets/geometry/GtsStaticVertex.h"
#include "assets/geometry/GtsGeometryMetadata.h"

struct GtsModelPrimitive;

namespace gtsGeometryPreparationDetail
{
    struct PrimitiveGeometry
    {
        std::vector<GtsStaticVertex>   vertices;
        std::vector<uint32_t> indices;
        MeshGeometryMetadata  metadata;
    };

    // Input must already pass primitive/profile validation. Skin streams are handled by the caller.
    PrimitiveGeometry preparePrimitiveGeometry(const GtsModelPrimitive& primitive);
} // namespace gtsGeometryPreparationDetail
