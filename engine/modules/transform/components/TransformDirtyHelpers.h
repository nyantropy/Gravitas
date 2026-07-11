#pragma once

#include <cstdint>
#include <limits>

#include "ECSWorld.hpp"
#include "TransformComponent.h"
#include "TransformInvalidationLifecycle.h"

namespace gts::transform
{
    inline uint32_t nextTransformVersion(uint32_t version)
    {
        return version == std::numeric_limits<uint32_t>::max() ? 1 : version + 1;
    }

    inline void markDirty(ECSWorld& world, Entity entity)
    {
        if (world.hasComponent<TransformComponent>(entity))
        {
            TransformComponent& transform = world.getComponent<TransformComponent>(entity);
            transform.version = nextTransformVersion(transform.version);
        }

        queueTransformDirty(world, entity);
    }
}
