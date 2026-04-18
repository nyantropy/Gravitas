#pragma once

#include <unordered_map>
#include <unordered_set>

#include "CameraDescriptionComponent.h"
#include "ECSWorld.hpp"

namespace gts::rendering
{
    struct CameraBindingLifecycleState
    {
        std::unordered_set<entity_id_type> refreshEntities;
        std::unordered_set<entity_id_type> cleanupEntities;
    };

    inline auto& cameraBindingLifecycleRegistry()
    {
        static std::unordered_map<ECSWorld*, CameraBindingLifecycleState> registry;
        return registry;
    }

    inline CameraBindingLifecycleState& cameraBindingLifecycleState(ECSWorld& world)
    {
        return cameraBindingLifecycleRegistry()[&world];
    }

    inline void resetCameraBindingLifecycleState(ECSWorld& world)
    {
        cameraBindingLifecycleRegistry().erase(&world);
    }

    inline std::unordered_set<entity_id_type> takeCameraRefreshes(ECSWorld& world)
    {
        auto& state = cameraBindingLifecycleState(world);
        std::unordered_set<entity_id_type> pending;
        pending.swap(state.refreshEntities);
        return pending;
    }

    inline std::unordered_set<entity_id_type> takeCameraCleanupEntities(ECSWorld& world)
    {
        auto& state = cameraBindingLifecycleState(world);
        std::unordered_set<entity_id_type> pending;
        pending.swap(state.cleanupEntities);
        return pending;
    }

    inline void queueCameraRefresh(ECSWorld& world, Entity entity)
    {
        cameraBindingLifecycleState(world).refreshEntities.insert(entity.id);
    }

    inline void queueCameraCleanup(ECSWorld& world, Entity entity)
    {
        cameraBindingLifecycleState(world).cleanupEntities.insert(entity.id);
    }
}
