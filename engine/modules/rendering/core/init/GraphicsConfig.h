#pragma once

#include <cstdint>

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
    int                maxFrameRate = 0; // 0 = uncapped
    uint32_t           maxScreenshotsPerRun = 64;
    float              minSecondsBetweenScreenshots = 0.25f;
    // Extend here later: antialiasing, render scale, shadow quality, etc.
};

struct RuntimeGraphicsSettings
{
    int width = 1920;
    int height = 1080;
    WindowMode windowMode = WindowMode::Windowed;
    bool vsync = true;
    PresentModePreference presentModePreference = PresentModePreference::Immediate;
    int maxFrameRate = 0; // 0 = uncapped
};
