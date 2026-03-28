#pragma once

#include "OutputWindowConfig.h"
#include "GraphicsBackend.h"

struct GraphicsConfig
{
    GraphicsBackend    backend = GraphicsBackend::Vulkan;
    OutputWindowConfig window;
    // Extend here later: antialiasing, render scale, shadow quality, etc.
};
