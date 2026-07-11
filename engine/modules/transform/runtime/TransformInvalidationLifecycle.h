#pragma once

#include <limits>
#include <unordered_map>
#include <vector>

#include "ECSWorld.hpp"

namespace gts::transform
{
    struct TransformInvalidationState
    {
        std::vector<entity_id_type> transformDirtyEntities;
        std::vector<uint8_t>        transformDirtyFlags;
    };

    inline auto& transformInvalidationRegistry()
    {
        static std::unordered_map<ECSWorld*, TransformInvalidationState> registry;
        return registry;
    }

    inline TransformInvalidationState& transformInvalidationState(ECSWorld& world)
    {
        return transformInvalidationRegistry()[&world];
    }

    inline void resetTransformInvalidationState(ECSWorld& world)
    {
        transformInvalidationRegistry().erase(&world);
    }

    inline bool validTransformEntity(Entity entity)
    {
        return entity.id != std::numeric_limits<entity_id_type>::max();
    }

    inline void ensureTransformDirtyFlagCapacity(std::vector<uint8_t>& flags, entity_id_type id)
    {
        const size_t index = static_cast<size_t>(id);
        if (index >= flags.size())
            flags.resize(index + 1, 0);
    }

    inline void queueTransformDirty(ECSWorld& world, Entity entity)
    {
        if (!validTransformEntity(entity))
            return;

        TransformInvalidationState& state = transformInvalidationState(world);
        ensureTransformDirtyFlagCapacity(state.transformDirtyFlags, entity.id);
        uint8_t& queued = state.transformDirtyFlags[static_cast<size_t>(entity.id)];
        if (queued != 0)
            return;

        queued = 1;
        state.transformDirtyEntities.push_back(entity.id);
    }

    inline void clearTransformDirtyQueue(TransformInvalidationState& state)
    {
        for (entity_id_type id : state.transformDirtyEntities)
        {
            const size_t index = static_cast<size_t>(id);
            if (index < state.transformDirtyFlags.size())
                state.transformDirtyFlags[index] = 0;
        }
        state.transformDirtyEntities.clear();
    }
}
