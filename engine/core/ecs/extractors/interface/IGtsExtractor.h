#pragma once

#include "GtsExtractorContext.h"

// Base interface for all ECS command extractors.
// Each extractor reads ECS components via GtsExtractorContext and produces
// a typed command output consumed by a render stage.
//
// Pattern: Component(s) → Extractor → CommandOutput → RenderStage
template<typename TOutput>
class IGtsExtractor
{
public:
    virtual ~IGtsExtractor() = default;

    // Extract commands for the current frame.
    // Called once per frame from GravitasEngine::render().
    virtual const TOutput& extract(const GtsExtractorContext& ctx) = 0;
};
