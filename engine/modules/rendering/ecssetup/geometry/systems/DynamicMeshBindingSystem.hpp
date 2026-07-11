#pragma once

#include "ECSControllerSystem.hpp"
#include "GeometryBindingLifecycle.h"
#include "DynamicMeshComponent.h"
#include "MeshBindingLifecycle.h"

class DynamicMeshBindingSystem : public ECSControllerSystem
{
public:
    void update(const EcsControllerContext& ctx) override
    {
        auto pendingMeshes = gts::rendering::takeDynamicMeshRefreshes(ctx.world);
        if (pendingMeshes.empty())
            return;

        auto& commands = ctx.world.commands();
        for (entity_id_type entityId : pendingMeshes)
        {
            Entity entity{entityId};
            if (!ctx.world.hasComponent<DynamicMeshComponent>(entity))
            {
                continue;
            }

            gts::rendering::syncDynamicMeshBinding(ctx.world, entity, ctx.resources, commands);
        }
    }
};
