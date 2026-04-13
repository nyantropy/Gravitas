#pragma once

#include "ECSWorld.hpp"
#include "TransformDirtyComponent.h"

namespace gts::transform
{
    inline void markDirty(ECSWorld& world, Entity entity)
    {
        if (!world.hasComponent<TransformDirtyComponent>(entity))
        {
            world.addComponent(entity, TransformDirtyComponent{});
            return;
        }

        world.getComponent<TransformDirtyComponent>(entity).dirty = true;
    }
}
