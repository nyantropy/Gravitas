#pragma once

#include <limits>
#include <unordered_map>
#include <vector>

#include "ECSWorld.hpp"

// Per-world render invalidation queues used to avoid full ECS scans in the
// transform upload and snapshot extraction hot paths.
namespace gts::rendering
{
    struct RenderInvalidationState
    {
        std::vector<entity_id_type> snapshotDirtyEntities;
        std::vector<uint8_t>        snapshotDirtyFlags;
    };

    inline auto& renderInvalidationRegistry(ECSWorld* world = nullptr)
    {
        // Static worlds may outlive this storage during process shutdown.
        static bool alive = true;
        struct Registry : std::unordered_map<ECSWorld*, RenderInvalidationState>
        {
            ~Registry() { alive = false; }
        };
        static Registry registry;
        if (world != nullptr && !registry.contains(world))
        {
            world->registerTeardownCallback([](ECSWorld& retiringWorld) noexcept
            {
                if (alive)
                    registry.erase(&retiringWorld);
            });
        }
        return registry;
    }

    inline RenderInvalidationState& renderInvalidationState(ECSWorld& world)
    {
        return renderInvalidationRegistry(&world)[&world];
    }

    inline void resetRenderInvalidationState(ECSWorld& world)
    {
        renderInvalidationRegistry().erase(&world);
    }

    inline bool validInvalidationEntity(Entity entity)
    {
        return entity.id != std::numeric_limits<entity_id_type>::max();
    }

    inline void ensureInvalidationFlagCapacity(std::vector<uint8_t>& flags, entity_id_type id)
    {
        const size_t index = static_cast<size_t>(id);
        if (index >= flags.size())
            flags.resize(index + 1, 0);
    }

    inline void queueRenderSnapshotDirty(ECSWorld& world, Entity entity)
    {
        if (!validInvalidationEntity(entity))
            return;

        RenderInvalidationState& state = renderInvalidationState(world);
        ensureInvalidationFlagCapacity(state.snapshotDirtyFlags, entity.id);
        uint8_t& queued = state.snapshotDirtyFlags[static_cast<size_t>(entity.id)];
        if (queued != 0)
            return;

        queued = 1;
        state.snapshotDirtyEntities.push_back(entity.id);
    }

    inline void clearSnapshotDirtyQueue(RenderInvalidationState& state)
    {
        for (entity_id_type id : state.snapshotDirtyEntities)
        {
            const size_t index = static_cast<size_t>(id);
            if (index < state.snapshotDirtyFlags.size())
                state.snapshotDirtyFlags[index] = 0;
        }
        state.snapshotDirtyEntities.clear();
    }
}
