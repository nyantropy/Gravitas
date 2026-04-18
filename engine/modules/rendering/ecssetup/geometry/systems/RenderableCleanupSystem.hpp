#pragma once

#include "ECSControllerSystem.hpp"
#include "GeometryBindingLifecycle.h"
#include "RenderGpuComponent.h"

// Shared geometry cleanup pass. This owns teardown of renderable GPU companion
// components regardless of whether the descriptor source was static mesh,
// procedural mesh, or world text.
class RenderableCleanupSystem : public ECSControllerSystem
{
public:
    void update(const EcsControllerContext& ctx) override
    {
        auto pendingCleanup = gts::rendering::takeRenderableCleanupEntities(ctx.world);
        if (pendingCleanup.empty())
            return;

        auto& commands = ctx.world.commands();
        for (entity_id_type entityId : pendingCleanup)
        {
            Entity entity{entityId};
            if (ctx.world.hasComponent<RenderGpuComponent>(entity)
                && !gts::rendering::hasRenderableDescriptor(ctx.world, entity))
            {
                gts::rendering::scheduleRenderableCleanup(ctx.world, commands, entity);
            }
        }
    }
};
