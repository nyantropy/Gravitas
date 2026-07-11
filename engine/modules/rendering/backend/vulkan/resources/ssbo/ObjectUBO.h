#pragma once

#include "GlmConfig.h"

struct ObjectUBO
{
    alignas(16) glm::mat4 model;
    alignas(16) glm::vec4 uvTransform = {1.0f, 1.0f, 0.0f, 0.0f};
    // Temporary scene-shader compatibility. Generic render extraction no
    // longer treats this as object-owned data; Vulkan fills it from material
    // frame state until base color moves into material descriptors.
    alignas(16) glm::vec4 tint        = {1.0f, 1.0f, 1.0f, 1.0f};
};
