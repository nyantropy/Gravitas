#pragma once

#include <cstdint>
#include <vector>

#include "GlmConfig.h"

namespace gts::debugdraw
{
    enum class DebugDrawColor
    {
        White,
        Red,
        Green,
        Blue,
        Cyan,
        Yellow,
        Orange,
        Purple,
        Grey
    };

    struct DebugDrawLine
    {
        glm::vec3 start{0.0f};
        glm::vec3 end{0.0f};
        DebugDrawColor color = DebugDrawColor::White;
        float thickness = 0.025f;
    };

    struct DebugDrawQueueComponent
    {
        std::vector<DebugDrawLine> lines;
        uint64_t version = 0;
    };
}
