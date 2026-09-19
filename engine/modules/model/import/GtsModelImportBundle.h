#pragma once

#include <memory>
#include <optional>
#include <vector>

#include "GtsModelAsset.h"
#include "GtsAnimationClipAsset.h"

struct GtsSkeletonAsset;

// one operations canonical products - model occurrences share these definition
// objects; this table enumerates definitions, not occurrences or compatibility
struct GtsModelImportBundle
{
    std::optional<GtsModelAsset>                         model;
    std::vector<std::shared_ptr<const GtsSkeletonAsset>> skeletons;
    std::vector<GtsAnimationClipAsset>                   animationClips;
};
