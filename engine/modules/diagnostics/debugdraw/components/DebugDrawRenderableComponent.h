#pragma once

#include "DebugDrawQueueComponent.h"

namespace gts::debugdraw
{
    struct DebugDrawRenderableComponent
    {
        DebugDrawColor color = DebugDrawColor::White;
        uint64_t geometryVersion = 0;
    };
}
