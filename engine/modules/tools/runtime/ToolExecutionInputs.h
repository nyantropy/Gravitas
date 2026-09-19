#pragma once

#include "RendererExecutionInputs.h"

namespace gts::tools
{
    struct ToolExecutionInputs
    {
        EcsSystemGroup                          timingGroup;
        gts::rendering::RendererExecutionInputs preview;
    };
} // namespace gts::tools
