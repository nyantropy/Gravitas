#pragma once

#include "ECSControllerSystem.hpp"
#include "GeometryBindingLifecycle.h"
#include "MeshBindingLifecycle.h"
#include "QuadMeshComponent.h"

class QuadMeshBindingSystem : public ECSControllerSystem
{
public:
    void update(const EcsControllerContext& ctx) override
    {
        auto pendingQuads = gts::rendering::takeQuadMeshRefreshes(ctx.world);
        if (pendingQuads.empty())
            return;

        auto& commands = ctx.world.commands();
        for (entity_id_type entityId : pendingQuads)
        {
            Entity entity{entityId};
            if (!ctx.world.hasComponent<QuadMeshComponent>(entity))
            {
                continue;
            }

            gts::rendering::syncQuadMeshBinding(ctx.world, entity, ctx.resources, commands);
        }
    }
};
