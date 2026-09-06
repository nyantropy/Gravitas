#pragma once

#include <cstdint>
#include <string>
#include "WindowBackend.h"
#include "OutputWindowConfig.h"

struct WindowManagerConfig
{
    WindowBackend windowBackend         = WindowBackend::GLFW;
    OutputWindowConfig window;
};
