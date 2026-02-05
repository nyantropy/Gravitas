#pragma once
#include <cstdint>

struct TimeContext
{
    float deltaTime;
    float unscaledDeltaTime;
    float timeScale = 1.0f;
    uint64_t frame;
};
