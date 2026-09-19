#include "TransformInvalidationLifecycle.h"

#include <cstddef>
#include <limits>
#include <unordered_map>
#include <vector>

#include "detail/TransformInvalidationState.h"

namespace gts::transform
{
    auto& transformInvalidationRegistry()
    {
        static std::unordered_map<ECSWorld*, TransformInvalidationState> registry;
        return registry;
    }

    auto& worldTransformPublishedCallbackRegistry()
    {
        static std::unordered_map<ECSWorld*, std::vector<WorldTransformPublishedCallback>> registry;
        return registry;
    }

    TransformInvalidationState& transformInvalidationState(ECSWorld& world)
    {
        return transformInvalidationRegistry()[&world];
    }

    void resetTransformInvalidationState(ECSWorld& world)
    {
        transformInvalidationRegistry().erase(&world);
    }

    void registerWorldTransformPublishedCallback(ECSWorld& world, WorldTransformPublishedCallback callback)
    {
        if (callback == nullptr)
            return;

        auto& callbacks = worldTransformPublishedCallbackRegistry()[&world];
        for (WorldTransformPublishedCallback existing : callbacks)
        {
            if (existing == callback)
                return;
        }
        callbacks.push_back(callback);
    }

    void notifyWorldTransformPublished(ECSWorld& world, Entity entity)
    {
        auto it = worldTransformPublishedCallbackRegistry().find(&world);
        if (it == worldTransformPublishedCallbackRegistry().end())
            return;

        for (WorldTransformPublishedCallback callback : it->second)
            callback(world, entity);
    }

    bool validTransformEntity(Entity entity)
    {
        return entity.id != std::numeric_limits<entity_id_type>::max();
    }

    void ensureTransformDirtyFlagCapacity(std::vector<uint8_t>& flags, entity_id_type id)
    {
        const size_t index = static_cast<size_t>(id);
        if (index >= flags.size())
            flags.resize(index + 1, 0);
    }

    void queueTransformDirty(ECSWorld& world, Entity entity)
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

    void clearTransformDirtyQueue(TransformInvalidationState& state)
    {
        for (entity_id_type id : state.transformDirtyEntities)
        {
            const size_t index = static_cast<size_t>(id);
            if (index < state.transformDirtyFlags.size())
                state.transformDirtyFlags[index] = 0;
        }
        state.transformDirtyEntities.clear();
    }
} // namespace gts::transform
