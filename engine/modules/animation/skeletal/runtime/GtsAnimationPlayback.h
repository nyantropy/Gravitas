#pragma once

#include <cstdint>
#include <optional>
#include "animation/skeletal/GtsSkeletonPose.h"

struct GtsAnimationClipAsset;
struct GtsSkeletonAsset;

// Mutable occurrence state; clip indices belong to the caller's immutable clip collection.
struct GtsAnimationPlayback
{
    std::optional<uint32_t> activeClip;
    double timeSeconds = 0;
    float speed = 1;
    bool looping = true;
    std::optional<GtsSkeletonPose> pose;

    void selectClip(uint32_t index);
};

// Explicit delta, no wall clock; publishes time/pose together only after successful evaluation.
void advanceGtsAnimationPlayback(GtsAnimationPlayback& playback, const GtsSkeletonAsset& skeleton,
                                 const GtsAnimationClipAsset& clip, double deltaSeconds);
