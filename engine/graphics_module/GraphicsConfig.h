#pragma once

#include <string>
#include <cstdint>

// contains simple graphics configuration variables
struct GraphicsConfig
{
    uint32_t outputWindowWidth;
    uint32_t outputWindowHeight;
    std::string outputWindowTitle;
};