#pragma once

#include <cstdint>

// Plain data struct written by multiple engine systems each frame and read
// by GtsDebugOverlay.  No ownership — passed by value via the frame graph
// data blackboard.
struct GtsFrameStats
{
    float    fps             = 0.0f;
    float    frameTimeMs     = 0.0f;
    uint32_t visibleObjects  = 0;
    uint32_t totalObjects    = 0;
    uint32_t triangleCount   = 0;
    uint32_t drawCalls       = 0;
    uint32_t pipelineSwitches = 0;
    uint32_t textureSwitches = 0;
    uint32_t physicsCollisionCount = 0;
    uint32_t playerCollisionCount  = 0;
};
