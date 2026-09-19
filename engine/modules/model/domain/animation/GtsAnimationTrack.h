#pragma once

#include <cstdint>
#include <limits>
#include <variant>
#include <vector>

#include "GlmConfig.h"
#include <gtc/quaternion.hpp>

enum class GtsAnimationTarget
{
    Translation,
    Rotation,
    Scale
};

enum class GtsAnimationInterpolation
{
    Step,
    Linear,
    CubicSpline
};

struct GtsAnimationCubicVec3Key
{
    glm::vec3 inTangent  = glm::vec3(0.0f);
    glm::vec3 value      = glm::vec3(0.0f);
    glm::vec3 outTangent = glm::vec3(0.0f);
};

struct GtsAnimationCubicRotationKey
{
    // derivatives of quaternion XYZW components per second, not rotations
    glm::vec4 inTangent  = glm::vec4(0.0f);
    glm::quat value      = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    glm::vec4 outTangent = glm::vec4(0.0f);
};

using GtsAnimationKeyValues = std::variant<std::vector<glm::vec3>,
                                           std::vector<glm::quat>,
                                           std::vector<GtsAnimationCubicVec3Key>,
                                           std::vector<GtsAnimationCubicRotationKey>>;

struct GtsAnimationTrack
{
    static constexpr uint32_t InvalidSkeletonNodeIndex = std::numeric_limits<uint32_t>::max();

    uint32_t                  skeletonNodeIndex = InvalidSkeletonNodeIndex;
    GtsAnimationTarget        target            = GtsAnimationTarget::Translation;
    GtsAnimationInterpolation interpolation     = GtsAnimationInterpolation::Linear;
    std::vector<float>        timesSeconds;
    // validation requires the typed alternative matching target and interpolation
    GtsAnimationKeyValues values;
};
