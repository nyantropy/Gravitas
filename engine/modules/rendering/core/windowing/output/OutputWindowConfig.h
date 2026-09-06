#pragma once

#include <string>

#include "WindowSettings.h"

// configuration settings for the OutputWindow wrapper object
struct OutputWindowConfig
{
    WindowSettings settings;
    std::string title = "Gravitas";

};
