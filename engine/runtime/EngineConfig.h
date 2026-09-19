#pragma once

#include <cstdint>

#include "GraphicsConfig.h"
#include "SimulationSettings.h"
#include "ToolSettings.h"

struct EngineConfig
{
    // maximum renderable objects, exceeding this number of objects will cause the engine to crash
    static constexpr uint32_t MAX_RENDERABLE_OBJECTS = 65536;

    // graphics settings
    GraphicsConfig graphics;

    // simulation settings for the simulation loop
    SimulationSettings simulation;

    // tooling settings for the engine toolchain
    ToolSettings tools;

    // Extend here later: audio config, input config, physics config, etc.
};
