#pragma once

#include <memory>
#include <span>
#include <vector>
#include "model/public/GtsModelClipReference.h"
#include "animation/skeletal/runtime/GtsAnimationPlayback.h"
#include "animation/skinning/GtsSkinPalette.h"

struct GtsModelBindingPalette
{
    uint32_t       skinBindingIndex = 0;
    GtsSkinPalette palette;
};

// Exclusively owned by a model instance. Immutable skeleton/clip definitions stay resource-owned.
class GtsSkeletonOccurrence
{
    public:
    uint32_t skeletonUseIndex() const
    {
        return useIndex;
    }
    const GtsSkeletonAsset& skeleton() const
    {
        return *definition;
    }
    const GtsSkeletonPose& pose() const
    {
        return *state.pose;
    }
    const GtsAnimationPlayback& playback() const
    {
        return state;
    }
    const std::optional<GtsModelClipReference>& activeClip() const
    {
        return clip;
    }
    std::span<const GtsModelBindingPalette> palettes() const
    {
        return bindingPalettes;
    }

    private:
    friend class GtsModelInstance;
    uint32_t                useIndex   = 0;
    const GtsSkeletonAsset* definition = nullptr;
    GtsAnimationPlayback    state; // Its pose is initialized even without an active clip; no second pose copy.
    std::optional<GtsModelClipReference> clip;
    std::vector<GtsModelBindingPalette>  bindingPalettes;
};

// Stable identity across updates. Does not keep either the instance or mutable pose alive.
// get() and the returned view are main-thread only; do not retain the raw view past owner destruction.
class GtsSkeletonOccurrenceReference
{
    public:
    const GtsSkeletonOccurrence* get() const
    {
        return lifetime.expired() ? nullptr : occurrence;
    }

    private:
    friend class GtsModelInstance;
    std::weak_ptr<const int>     lifetime;
    const GtsSkeletonOccurrence* occurrence = nullptr;
};
