#pragma once

#include <cstdint>
#include <string>

struct WindowManagerConfig
{
    uint32_t    windowWidth             = 1920;
    uint32_t    windowHeight            = 1080;
    std::string windowTitle             = "Gravitas";
    bool        enableValidationLayers  = false;
    bool        borderlessFullscreen    = false;
};
