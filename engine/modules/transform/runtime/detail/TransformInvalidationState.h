#pragma once

#include <cstdint>
#include <vector>

#include "Entity.h"

class ECSWorld;

namespace gts::transform
{
    struct TransformInvalidationState
    {
        std::vector<entity_id_type> transformDirtyEntities;
        std::vector<uint8_t>        transformDirtyFlags;
    };

    TransformInvalidationState& transformInvalidationState(ECSWorld& world);
    void                        clearTransformDirtyQueue(TransformInvalidationState& state);
    void                        notifyWorldTransformPublished(ECSWorld& world, Entity entity);
} // namespace gts::transform
