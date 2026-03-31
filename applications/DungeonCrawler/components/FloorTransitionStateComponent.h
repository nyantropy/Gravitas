#pragma once

#include "GlmConfig.h"
#include "FloorTransitionType.h"

// Singleton component — one per scene.
// FloorTransitionSystem reads and writes this each frame.
// PlayerMovementSystem and PlayerCameraSystem return early while active.
struct FloorTransitionStateComponent
{
    bool                active      = false;
    FloorTransitionType type        = FloorTransitionType::Stairs;
    float               progress    = 0.0f;   // [0..1]
    int                 targetFloor = 0;
    glm::ivec2          arrivalGrid = {};

    // Camera animation endpoints
    glm::vec3 camStart    = {};
    glm::vec3 camEnd      = {};
    glm::vec3 lookAtStart = {};
    glm::vec3 lookAtEnd   = {};

    static constexpr float STAIRS_DURATION   = 1.2f;
    static constexpr float HOLE_DURATION     = 0.8f;
    static constexpr float COLLAPSE_DURATION = 1.5f;

    float getDuration() const
    {
        switch (type)
        {
            case FloorTransitionType::Stairs:   return STAIRS_DURATION;
            case FloorTransitionType::Hole:     return HOLE_DURATION;
            case FloorTransitionType::Collapse: return COLLAPSE_DURATION;
            default: return STAIRS_DURATION;
        }
    }
};
