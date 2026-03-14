#pragma once

#include <string>

#include "WindowMode.h"

// configuration settings for the OutputWindow wrapper object
struct OutputWindowConfig
{
    int width  = 1920;
    int height = 1080;
    std::string title = "Gravitas";

    bool       enableValidationLayers = false;
    WindowMode windowMode             = WindowMode::Windowed;
    bool       vsync                  = true;
};
