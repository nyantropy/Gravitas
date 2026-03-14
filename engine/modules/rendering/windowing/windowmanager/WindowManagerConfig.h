#pragma once

#include <cstdint>
#include <string>
#include "WindowBackend.h"
#include "OutputWindowConfig.h"

struct WindowManagerConfig
{
    WindowBackend windowBackend         = WindowBackend::GLFW;
    uint32_t    windowWidth             = 1920;
    uint32_t    windowHeight            = 1080;
    std::string windowTitle             = "Gravitas";
    bool        enableValidationLayers  = false;
    WindowMode  windowMode              = WindowMode::Windowed;
};
