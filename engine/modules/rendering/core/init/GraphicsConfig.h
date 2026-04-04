#pragma once

#include "../windowing/output/OutputWindowConfig.h"
#include "../../../../core/options/GraphicsBackend.h"

enum class PresentModePreference
{
    Immediate = 0,
    Mailbox,
    Fifo
};

struct GraphicsConfig
{
    GraphicsBackend    backend = GraphicsBackend::Vulkan;
    bool               enableValidationLayers = true;
    OutputWindowConfig window;
    PresentModePreference presentModePreference = PresentModePreference::Immediate;
    // Extend here later: antialiasing, render scale, shadow quality, etc.
};
