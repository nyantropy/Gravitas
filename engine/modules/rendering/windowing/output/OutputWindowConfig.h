#pragma once

#include <string>

// configuration settings for the OutputWindow wrapper object
struct OutputWindowConfig
{
    int width  = 1920;
    int height = 1080;
    std::string title = "Gravitas";

    bool enableValidationLayers = false;
    bool borderlessFullscreen   = false;
    bool vsync                  = true;
    int  monitorIndex           = 0;    // 0 = primary, 1 = second monitor, etc.
};
