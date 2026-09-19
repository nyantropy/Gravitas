#pragma once

#include <cstdint>

struct GtsFrameEndedEvent
{
    float    dt         = 0.0f;
    uint32_t imageIndex = 0;
};
