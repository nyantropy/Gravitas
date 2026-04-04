#pragma once

#include "GlmConfig.h"

// Runtime interpolation state for grid-based enemy movement.
struct EnemyMovementStateComponent
{
    glm::vec3  startPosition  = {0.0f, 0.0f, 0.0f};
    glm::vec3  targetPosition = {0.0f, 0.0f, 0.0f};
    glm::ivec2 targetTile     = {0, 0};
    float      progress       = 0.0f;
    bool       moving         = false;
};
