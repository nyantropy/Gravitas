#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "GtsSkeletonTypes.h"

struct GtsSkeletonNode
{
    GtsSkeletonNodeId         id;
    std::string               name;
    std::optional<uint32_t>   parentIndex;
    GtsSkeletonLocalTransform defaultLocalTransform;
};

// reusable asset definition
struct GtsSkeletonAsset
{
    std::string                  name;
    std::vector<GtsSkeletonNode> nodes;
};

// both assets must be valid - compares ordered IDs, parents, and exact stored
// transform forms/components; asset and node display names are ignored
[[nodiscard]] bool areGtsSkeletonsCompatible(const GtsSkeletonAsset& left, const GtsSkeletonAsset& right);
