#pragma once
#include <cstdint>

struct TimeContext
{
    float deltaTime = 0.0f;
    float unscaledDeltaTime = 0.0f;
    float timeScale = 1.0f;
    float simulationAlpha = 0.0f;
    uint64_t frame = 0;
};
