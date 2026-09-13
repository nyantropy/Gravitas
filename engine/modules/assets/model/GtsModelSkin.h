#pragma once

#include <cstdint>
#include <limits>
#include <memory>

#include "assets/skin/GtsSkinBinding.h"

struct GtsSkeletonAsset;

// each entry is a distinct occurrence, even when definitions are shared
// persistent asset identity and mutable pose state are not part of this value
struct GtsModelSkeletonUse
{
    std::shared_ptr<const GtsSkeletonAsset> skeleton;
};

struct GtsModelSkinBinding
{
    static constexpr uint32_t InvalidSkeletonUseIndex = std::numeric_limits<uint32_t>::max();

    GtsSkinBinding binding;
    uint32_t       skeletonUseIndex = InvalidSkeletonUseIndex;
};
