#pragma once

#include "core/skinning/SkinnedFrameData.h"

// CPU presentation descriptor. Vulkan resources are owned by the backend scene renderer.
struct SkinnedModelComponent
{
    std::shared_ptr<const GtsSkinnedModelInstance> instance;
    std::shared_ptr<const std::vector<GtsSkinPalette>> palettes;
    glm::mat4 actorFromReference{1};
};
