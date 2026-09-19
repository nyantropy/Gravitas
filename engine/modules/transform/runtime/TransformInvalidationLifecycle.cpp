#include "TransformInvalidationLifecycle.h"

#include <cstddef>
#include <limits>
#include <unordered_map>
#include <vector>

#include "detail/TransformInvalidationState.h"
#include "ECSWorld.hpp"

namespace gts::transform
{
    namespace
    {
        struct TransformWorldRegistries;
        TransformWorldRegistries* liveRegistries = nullptr;

        struct TransformWorldRegistries
        {
            std::unordered_map<const ECSWorld*, TransformInvalidationState>                   invalidation;
            std::unordered_map<const ECSWorld*, std::vector<WorldTransformPublishedCallback>> publication;

            TransformWorldRegistries()
            {
                liveRegistries = this;
            }

            ~TransformWorldRegistries()
            {
                liveRegistries = nullptr;
            }
        };

        TransformWorldRegistries& registries()
        {
            static TransformWorldRegistries state;
            return state;
        }

        auto& transformInvalidationRegistry()
        {
            return registries().invalidation;
        }

        auto& worldTransformPublishedCallbackRegistry()
        {
            return registries().publication;
        }
    } // namespace

    void installTransformWorldState(ECSWorld& world)
    {
        world.registerTeardownCallback(releaseTransformWorldState);
        transformInvalidationRegistry().try_emplace(&world);
        worldTransformPublishedCallbackRegistry().try_emplace(&world);
    }

    void releaseTransformWorldState(ECSWorld& world) noexcept
    {
        // A static world may outlive the registries during process shutdown.
        if (liveRegistries == nullptr)
            return;
        liveRegistries->invalidation.erase(&world);
        liveRegistries->publication.erase(&world);
    }

    TransformInvalidationState& transformInvalidationState(ECSWorld& world)
    {
        auto& registry = transformInvalidationRegistry();
        auto  it       = registry.find(&world);
        if (it == registry.end())
        {
            installTransformWorldState(world);
            it = registry.find(&world);
        }
        return it->second;
    }

    void registerWorldTransformPublishedCallback(ECSWorld& world, WorldTransformPublishedCallback callback)
    {
        if (callback == nullptr)
            return;

        installTransformWorldState(world);
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

    TransformWorldStateInspection inspectTransformWorldState(const ECSWorld* world)
    {
        const auto&                   invalidation = transformInvalidationRegistry();
        const auto&                   publication  = worldTransformPublishedCallbackRegistry();
        TransformWorldStateInspection result;
        result.invalidationWorlds = invalidation.size();
        result.publicationWorlds  = publication.size();
        if (const auto it = invalidation.find(world); it != invalidation.end())
        {
            result.hasInvalidation = true;
            result.queuedEntities  = it->second.transformDirtyEntities.size();
            result.dirtyFlags      = it->second.transformDirtyFlags.size();
        }
        if (const auto it = publication.find(world); it != publication.end())
        {
            result.hasPublication = true;
            result.callbacks      = it->second.size();
        }
        return result;
    }
} // namespace gts::transform
