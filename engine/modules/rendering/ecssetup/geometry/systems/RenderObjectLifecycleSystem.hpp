#pragma once

#include "ECSControllerSystem.hpp"
#include "GeometryBindingLifecycle.h"
#include "RenderObjectLifecycle.h"

class RenderObjectLifecycleSystem : public ECSControllerSystem
{
public:
    void update(const EcsControllerContext& ctx) override
    {
        auto pendingRenderObjects = gts::rendering::takeRenderObjectRefreshes(ctx.world);
        if (pendingRenderObjects.empty())
            return;

        auto& commands = ctx.world.commands();
        for (entity_id_type entityId : pendingRenderObjects)
            gts::rendering::syncRenderObjectLifecycle(ctx.world, Entity{entityId}, ctx.resources, commands);
    }
};
