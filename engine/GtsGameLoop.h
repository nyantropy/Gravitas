#pragma once

#include "EngineConfig.h"

// Fixed-timestep accumulator for the engine game loop.
// Owned by GravitasEngine; no heap allocations.
struct GtsGameLoop
{
    float fixedTimestep = 1.0f / 20.0f;  // derived from EngineConfig::simulationTickRate
    float accumulator   = 0.0f;
    bool  paused        = false;

    // Initialise from config. Call once before the loop starts.
    void init(const EngineConfig& config)
    {
        fixedTimestep = 1.0f / static_cast<float>(config.simulationTickRate);
        accumulator   = 0.0f;
        paused        = false;
    }

    // Advance the accumulator by realDt. Returns the number of simulation
    // ticks that should fire this frame (0 or more).
    int advance(float realDt)
    {
        if (paused) return 0;
        accumulator += realDt;
        int ticks = 0;
        while (accumulator >= fixedTimestep)
        {
            accumulator -= fixedTimestep;
            ++ticks;
        }
        return ticks;
    }

    // The fractional remainder as a 0..1 alpha for render interpolation.
    float alpha() const
    {
        return accumulator / fixedTimestep;
    }

    // Compute the deltaTime value exposed to simulation systems.
    // Returns fixedTimestep when running, 0.0f when paused.
    float simulationDt() const
    {
        return paused ? 0.0f : fixedTimestep;
    }
};
