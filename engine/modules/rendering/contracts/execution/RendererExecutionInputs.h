#pragma once

#include "EcsExecutionSelection.h"

namespace gts::rendering
{
    struct RendererExecutionInputs
    {
        EcsExecutionSelection defaultSelection;
        EcsSystemGroup        preparation;
        EcsSystemGroup        textureAnimation;
        EcsSystemGroup        camera;
        EcsSystemGroup        particles;
    };
} // namespace gts::rendering
