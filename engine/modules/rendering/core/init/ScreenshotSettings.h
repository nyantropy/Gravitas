#pragma once

#include <cstdint>

// settings for screenshots
struct ScreenshotSettings
{
    // screenshot capture limit (per run)
    uint32_t maxScreenshotsPerRun = 64;

    // time that needs to pass before another screenshot can be taken
    float minSecondsBetweenScreenshots = 0.25f;
};
