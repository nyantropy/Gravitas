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
    bool               headless = false;
    OutputWindowConfig window;
    PresentModePreference presentModePreference = PresentModePreference::Immediate;
    uint32_t           maxScreenshotsPerRun = 64;
    float              minSecondsBetweenScreenshots = 0.25f;
    uint32_t           autoScreenshotDelayFrames = 0;
    // Extend here later: antialiasing, render scale, shadow quality, etc.
};
