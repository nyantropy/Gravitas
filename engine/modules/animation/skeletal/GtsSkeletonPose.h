#pragma once

#include <vector>

#include "assets/skeleton/GtsSkeletonCompatibility.h"

// evaluated occurrence data, indexed exactly like the supplied skeleton's nodes
struct GtsSkeletonPose
{
    // Exact indexing contract, independent of skeleton object lifetime or display names.
    GtsSkeletonCompatibility skeletonCompatibility;
    std::vector<GtsSkeletonLocalTransform> localTransforms;
    // node-local coordinates -> skeleton reference space; no world placement or inverse binds
    std::vector<glm::mat4> modelTransforms;
};
