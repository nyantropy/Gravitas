#pragma once

#include "GraphicsConfig.h"

struct EngineConfig
{
    GraphicsConfig graphics;

    // Number of simulation ticks per second.
    // Controller systems and rendering always run every frame.
    // Default: 20 ticks/sec (one tick = 0.05 s of game time).
    int simulationTickRate = 20;

    // Extend here later: audio config, input config, physics config, etc.
};
