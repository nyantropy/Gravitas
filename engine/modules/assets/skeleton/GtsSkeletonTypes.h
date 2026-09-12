#pragma once

#include <string>
#include <variant>

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
