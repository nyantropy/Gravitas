#pragma once

struct SimulationSettings
{
    // Number of simulation ticks per second.
    // Controller systems and rendering always run every frame.
    // Default: 20 ticks/sec (one tick = 0.05 s of game time).
    int tickRate = 20;
};
