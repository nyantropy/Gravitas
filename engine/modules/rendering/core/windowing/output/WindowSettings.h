#pragma once

#include <string>

#include "WindowMode.h"

// settings for the output window
struct WindowSettings
{
    int width = 1920;
    int height = 1080;
    WindowMode windowMode = WindowMode::Windowed;
    int monitorIndex = 0;
    std::string monitorName;

    bool operator==(const WindowSettings&) const = default;
};
