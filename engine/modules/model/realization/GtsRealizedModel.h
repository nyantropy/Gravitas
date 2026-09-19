#pragma once
#include "GtsRealizedGeometry.h"
#include "GtsModelHandle.h"

struct GtsRealizedModelOccurrence
{
    uint32_t                modelNodeIndex = 0;
    uint32_t                geometryIndex  = 0;
    std::optional<uint32_t> skinBindingIndex;
    std::optional<uint32_t> skeletonUseIndex;
};

// Shared derived definition. Hierarchy, bindings, materials and dependencies stay in model.
struct GtsRealizedModel
{
    GtsModelHandle                          model;
    std::vector<GtsRealizedGeometry>        geometry;
    std::vector<GtsRealizedModelOccurrence> occurrences;
};
