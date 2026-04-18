#pragma once

#include "CameraDescriptionComponent.h"
#include "CameraGpuComponent.h"
#include "CameraBindingLifecycle.h"
#include "ECSControllerSystem.hpp"

// Controller system that owns the descriptor -> GPU companion lifecycle for cameras.
// It only processes queued camera entities and performs structural mutation through
// the world command buffer so steady-state frames are O(1).
class CameraLifecycleSystem : public ECSControllerSystem
{
public:
    void update(const EcsControllerContext& ctx) override
    {
        auto pendingCleanup = gts::rendering::takeCameraCleanupEntities(ctx.world);
        auto pendingRefresh = gts::rendering::takeCameraRefreshes(ctx.world);

        if (pendingCleanup.empty() && pendingRefresh.empty())
            return;

        auto& commands = ctx.world.commands();

        for (entity_id_type entityId : pendingCleanup)
        {
            Entity entity{ entityId };
            const bool refreshQueued = pendingRefresh.contains(entityId);
            const bool hasDescriptor = ctx.world.hasComponent<CameraDescriptionComponent>(entity);

            // Descriptor churn can queue cleanup + refresh in the same frame.
            // Keep the existing GPU companion alive when the descriptor still
            // exists so we do not create a one-frame gap between removal and
            // re-creation after deferred commands flush.
            if (refreshQueued && hasDescriptor)
                continue;

            if (ctx.world.hasComponent<CameraGpuComponent>(entity))
                commands.removeComponent<CameraGpuComponent>(entity);
        }

        for (entity_id_type entityId : pendingRefresh)
        {
            Entity entity{ entityId };
            if (!ctx.world.hasComponent<CameraDescriptionComponent>(entity)
                || ctx.world.hasComponent<CameraGpuComponent>(entity))
            {
                continue;
            }

            commands.addComponent(entity, CameraGpuComponent{});
        }
    }
};
