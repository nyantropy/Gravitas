#pragma once

#include <cstdint>
#include <string>

struct WindowManagerConfig
{
    uint32_t windowWidth;
    uint32_t windowHeight;
    std::string windowTitle;

    bool enableValidationLayers;
};