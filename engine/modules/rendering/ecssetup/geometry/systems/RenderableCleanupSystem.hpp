#pragma once

#include "ECSControllerSystem.hpp"
#include "GeometryBindingLifecycle.h"
#include "MeshGpuComponent.h"
#include "RenderGpuComponent.h"
#include "RenderObjectLifecycle.h"

// Shared geometry cleanup pass. This owns teardown of renderable GPU companion
// components regardless of whether the descriptor source was static mesh,
// quad mesh, dynamic mesh, or world text.
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
            if (!gts::rendering::hasRenderableGeometryDescriptor(ctx.world, entity))
                gts::rendering::scheduleMeshGpuCleanup(ctx.world, commands, entity);

            if (!gts::rendering::renderObjectReady(ctx.world, entity))
                gts::rendering::scheduleRenderObjectCleanup(ctx.world, commands, entity);
        }
    }
};
