#include <cmath>
#include <cstdio>

#include "ProfileAccumulator.h"

namespace
{
    bool require(bool condition, const char* message)
    {
        if (condition)
            return true;

        std::fprintf(stderr, "FAIL: %s\n", message);
        return false;
    }

    bool near(float lhs, float rhs, float epsilon = 0.0001f)
    {
        return std::fabs(lhs - rhs) <= epsilon;
    }
}

int main()
{
    bool ok = true;

    ProfileAccumulator accumulator;

    GtsFrameStats first;
    first.fps = 1000.0f;
    first.frameTimeMs = 1.0f;
    first.frameCpuMs = 2.0f;
    first.screenshotRequestedCount = 1;
    first.screenshotScheduledCount = 1;
    first.screenshotPendingGpuCount = 1;
    first.screenshotScheduleCpuMs = 0.25f;

    GtsFrameStats second;
    second.fps = 10.0f;
    second.frameTimeMs = 100.0f;
    second.frameCpuMs = 4.0f;
    second.screenshotCompletedCount = 1;
    second.screenshotPendingWriteCount = 1;
    second.screenshotReadbackBytes = 128;
    second.screenshotReadbackCpuMs = 0.5f;

    accumulator.add(first, 0.010f);
    accumulator.add(second, 0.020f);

    ok &= require(accumulator.frameCount == 2, "frame count accumulates");
    ok &= require(near(accumulator.averageFrameDeltaMs(), 15.0f),
                  "average frame delta is derived from the profile window");
    ok &= require(near(accumulator.windowFps(), 66.6667f, 0.001f),
                  "window fps is derived from elapsed window time");
    ok &= require(accumulator.max.frameTimeMs == 100.0f,
                  "max frame delta remains available");
    ok &= require(accumulator.sum.screenshotRequestedCount == 1,
                  "screenshot requests accumulate");
    ok &= require(accumulator.sum.screenshotScheduledCount == 1,
                  "scheduled screenshot captures accumulate");
    ok &= require(accumulator.sum.screenshotCompletedCount == 1,
                  "completed screenshot captures accumulate");
    ok &= require(accumulator.max.screenshotPendingGpuCount == 1,
                  "pending gpu capture high-water mark is preserved");
    ok &= require(accumulator.max.screenshotPendingWriteCount == 1,
                  "pending write high-water mark is preserved");
    ok &= require(accumulator.sum.screenshotReadbackBytes == 128,
                  "screenshot readback bytes accumulate");

    accumulator.reset();
    ok &= require(accumulator.frameCount == 0, "reset clears frame count");
    ok &= require(accumulator.timeAccum == 0.0f, "reset clears elapsed time");
    ok &= require(accumulator.windowFps() == 0.0f, "empty fps is zero");
    ok &= require(accumulator.averageFrameDeltaMs() == 0.0f, "empty frame delta is zero");

    return ok ? 0 : 1;
}
