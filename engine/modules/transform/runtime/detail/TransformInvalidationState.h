#pragma once

#include <cstdint>
#include <cstddef>
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

    void installTransformWorldState(ECSWorld& world);
    void releaseTransformWorldState(ECSWorld& world) noexcept;

    struct TransformWorldStateInspection
    {
        size_t invalidationWorlds = 0;
        size_t publicationWorlds  = 0;
        bool   hasInvalidation    = false;
        bool   hasPublication     = false;
        size_t queuedEntities     = 0;
        size_t dirtyFlags         = 0;
        size_t callbacks          = 0;
    };

    TransformWorldStateInspection inspectTransformWorldState(const ECSWorld* world = nullptr);

    TransformInvalidationState& transformInvalidationState(ECSWorld& world);
    void                        clearTransformDirtyQueue(TransformInvalidationState& state);
    void                        notifyWorldTransformPublished(ECSWorld& world, Entity entity);
} // namespace gts::transform
