#pragma once

#include "ECSWorld.hpp"
#include "RenderDirtyComponent.h"
#include "RenderGpuComponent.h"

namespace gts::transform
{
    inline void markDirty(ECSWorld& world, Entity entity)
    {
        if (world.hasComponent<RenderGpuComponent>(entity))
        {
            auto& renderGpu = world.getComponent<RenderGpuComponent>(entity);
            renderGpu.dirty         = true;
            renderGpu.readyToRender = false;
            renderGpu.commandDirty  = true;
        }

        if (world.hasComponent<RenderDirtyComponent>(entity))
            world.getComponent<RenderDirtyComponent>(entity).transformDirty = true;
    }
}
