#pragma once

#include <string>

#include "WindowSettings.h"
#include "PresentationSettings.h"
#include "RenderSettings.h"
#include "time/FramePacingSettings.h"

// actual graphics settings, changing these changes how the engine behaves
struct GraphicsSettings
{
    WindowSettings window;
    PresentationSettings presentation;
    RenderSettings rendering;
    FramePacingSettings framePacing;
};

inline std::string validateGraphicsSettings(const GraphicsSettings& settings)
{
    if (settings.window.width <= 0 || settings.window.height <= 0)
        return "Window dimensions must be positive";
    if (settings.window.monitorIndex < 0)
        return "Monitor index must be nonnegative";
    if (settings.window.windowMode != WindowMode::Windowed
        && settings.window.windowMode != WindowMode::BorderlessFullscreen
        && settings.window.windowMode != WindowMode::Fullscreen)
        return "Invalid window mode";
    if (settings.presentation.mode != PresentModePreference::Immediate
        && settings.presentation.mode != PresentModePreference::Mailbox
        && settings.presentation.mode != PresentModePreference::Fifo)
        return "Invalid presentation policy";
    if (settings.framePacing.maxFrameRate < 0)
        return "Frame rate limit must be nonnegative";
    if (!resolveRenderExtent(settings.rendering.resolution,
                             {static_cast<uint32_t>(settings.window.width),
                              static_cast<uint32_t>(settings.window.height)}))
        return "Invalid render resolution policy";
    return {};
}
