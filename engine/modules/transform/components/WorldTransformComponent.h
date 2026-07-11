#pragma once

#include "GlmConfig.h"

// Resolved scene-space transform. TransformSystem is the only writer.
struct WorldTransformComponent
{
    glm::mat4 matrix  = glm::mat4(1.0f);
    uint32_t  version = 1;
};
