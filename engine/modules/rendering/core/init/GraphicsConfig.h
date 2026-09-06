#pragma once

#include <string>

#include "GraphicsStartupOptions.h"
#include "GraphicsSettings.h"
#include "ScreenshotSettings.h"

struct GraphicsConfig
{
    GraphicsStartupOptions startup;
    GraphicsSettings settings;
    std::string windowTitle = "Gravitas";
    ScreenshotSettings screenshots;
    // Extend here later: antialiasing, render scale, shadow quality, etc.
};
