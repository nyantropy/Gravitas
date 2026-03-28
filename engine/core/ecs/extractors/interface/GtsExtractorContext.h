#pragma once

#include "ECSWorld.hpp"

// Context passed to every extractor each frame.
// Add fields here when extractors need additional per-frame data
// without changing the IGtsExtractor interface signature.
struct GtsExtractorContext
{
    ECSWorld& world;
    float     viewportAspect = 1.0f;
    // future: const TimeContext* time, active camera handle, etc.
};
