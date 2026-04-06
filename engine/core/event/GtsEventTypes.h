#pragma once

#include <cstdint>

struct GtsWindowResizeEvent
{
    int width  = 0;
    int height = 0;
};

struct GtsFrameEndedEvent
{
    float    dt         = 0.0f;
    uint32_t imageIndex = 0;
};
