#pragma once

#include "GraphicsConfig.h"
#include "SimulationSettings.h"
#include "ToolSettings.h"

struct EngineConfig
{
    // graphics settings
    GraphicsConfig graphics;

    // simulation settings for the simulation loop
    SimulationSettings simulation;

    // tooling settings for the engine toolchain
    ToolSettings tools;

    // Extend here later: audio config, input config, physics config, etc.
};
