#pragma once

#include <cstdint>
#include <limits>
#include <string>
#include <vector>

#include "GlmConfig.h"

#include "assets/skeleton/GtsSkeletonCompatibility.h"

struct GtsSkinJointBinding
{
    static constexpr uint32_t InvalidSkeletonNodeIndex = std::numeric_limits<uint32_t>::max();

    uint32_t skeletonNodeIndex = InvalidSkeletonNodeIndex;
    // stored mesh vertex coordinates -> binding coordinates of the mapped node
    // this is authored binding data, never derived from the skeleton default pose
    glm::mat4 inverseBindMatrix = glm::mat4(1.0f);
};

struct GtsSkinBinding
{
    std::string name;
    // structural expectation only; selection of a particular asset belongs to
    // future model associations, an empty descriptor is invalid
    GtsSkeletonCompatibility targetSkeletonCompatibility;
    // array position is the skin-local slot; mappings need not be unique/sorted
    std::vector<GtsSkinJointBinding> joints;
};
