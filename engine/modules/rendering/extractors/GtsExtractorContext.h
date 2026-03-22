#pragma once

#include "ECSWorld.hpp"
#include "GtsFrameStats.h"

// Context passed to every extractor each frame.
// Add fields here when extractors need additional per-frame data
// without changing the IGtsExtractor interface signature.
struct GtsExtractorContext
{
    ECSWorld& world;
    float     viewportAspect = 1.0f;
    const GtsFrameStats* frameStats = nullptr;
    // future: const TimeContext* time, active camera handle, etc.
};
