#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "GlmConfig.h"

#include <gtc/quaternion.hpp>

// opaque, case-sensitive identity within a skeleton - producers retain this value
// across renaming/reordering
struct GtsSkeletonNodeId
{
    std::string value;

    bool operator==(const GtsSkeletonNodeId&) const = default;
};

struct GtsSkeletonTrs
{
    glm::vec3 translation = glm::vec3(0.0f);
    glm::quat rotation    = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    glm::vec3 scale       = glm::vec3(1.0f);
};

// defaults to identity TRS - matrix values preserve fixed affine transforms
// exactly, without decomposition or a redundant TRS representation
using GtsSkeletonLocalTransform = std::variant<GtsSkeletonTrs, glm::mat4>;

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
