#pragma once

#include <cstdint>
#include <string>

#include "../windowing/output/OutputWindowConfig.h"
#include "../windowing/output/GraphicsMonitorInfo.h"

enum class GraphicsBackend
{
    Vulkan
};

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
    uint32_t           renderWidth = 0;  // 0 = follow initial window size
    uint32_t           renderHeight = 0; // 0 = follow initial window size
    PresentModePreference presentModePreference = PresentModePreference::Immediate;
    int                maxFrameRate = 0; // 0 = uncapped
    uint32_t           maxScreenshotsPerRun = 64;
    float              minSecondsBetweenScreenshots = 0.25f;
    // Extend here later: antialiasing, render scale, shadow quality, etc.
};

struct RuntimeGraphicsSettings
{
    // Requested game/render resolution. Windowed and exclusive fullscreen use
    // this as the output size too; borderless keeps desktop output and upscales
    // this internal render target to the swapchain.
    int width = 1920;
    int height = 1080;
    WindowMode windowMode = WindowMode::Windowed;
    bool vsync = true;
    PresentModePreference presentModePreference = PresentModePreference::Immediate;
    int maxFrameRate = 0; // 0 = uncapped
    int monitorIndex = 0;
    std::string monitorName;
};
