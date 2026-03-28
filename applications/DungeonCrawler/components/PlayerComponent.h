#pragma once

#include "GlmConfig.h"

// Grid-space player state for the dungeon crawler.
// Position is in grid cells. Facing: 0=North(-Z), 1=East(+X), 2=South(+Z), 3=West(-X).
struct PlayerComponent
{
    // Logical state — updated immediately on input
    int gridX  = 3;
    int gridZ  = 2;
    int facing = 1; // 0=North, 1=East, 2=South, 3=West

    // Transition state — drives smooth visual interpolation
    bool  inTransition       = false;
    float transitionProgress = 0.0f;

    glm::vec3 fromPosition = {3.5f, 0.5f, 2.5f};
    glm::vec3 toPosition   = {3.5f, 0.5f, 2.5f};
    float     fromYaw      = 90.0f; // East
    float     toYaw        = 90.0f;

    static constexpr float TRANSITION_DURATION = 0.15f;
};
