#pragma once

#include "GlmConfig.h"

struct ObjectUBO
{
    alignas(16) glm::mat4 model;
    alignas(16) glm::vec4 uvTransform = {1.0f, 1.0f, 0.0f, 0.0f};
    alignas(16) glm::vec4 tint        = {1.0f, 1.0f, 1.0f, 1.0f};
};
