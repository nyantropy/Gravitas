#pragma once

#include "Entity.h"
class ECSWorld;

namespace gts::rendering
{
    struct RenderableRuntimeStatus
    {
        bool meshGpuReady          = false;
        bool materialGpuReady      = false;
        bool renderObjectAllocated = false;
        bool renderReady           = false;
    };

    RenderableRuntimeStatus renderableRuntimeStatus(const ECSWorld& world, Entity entity);
} // namespace gts::rendering
