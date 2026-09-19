#pragma once

#include <string>
#include <vector>

#include "GtsAnimationTrack.h"
#include "model/domain/skeleton/GtsSkeletonCompatibility.h"

// canonical asset data; consumers leave it unchanged and own playback state separately
struct GtsAnimationClipAsset
{
    std::string                    name;
    GtsSkeletonCompatibility       targetSkeletonCompatibility;
    float                          durationSeconds = 0.0f;
    std::vector<GtsAnimationTrack> tracks;
};
