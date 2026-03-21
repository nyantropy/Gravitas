#pragma once

#include "GraphicsConfig.h"

struct EngineConfig
{
    GraphicsConfig graphics;

    // Number of simulation ticks per second.
    // Controller systems and rendering always run every frame.
    // Default: 20 ticks/sec (one tick = 0.05 s of game time).
    int simulationTickRate = 20;

    // Enable frustum culling in RenderCommandExtractor.
    // Entities without a BoundsComponent are never culled regardless of this flag.
    // Set to false to disable culling globally (useful for debugging).
    bool frustumCullingEnabled = true;

    // Whether the F3 debug overlay is visible by default on startup.
    bool debugOverlayEnabledByDefault = false;

    // Extend here later: audio config, input config, physics config, etc.
};
