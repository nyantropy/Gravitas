#pragma once

#include "ECSControllerSystem.hpp"
#include "QuadMeshComponent.h"
#include "MaterialComponent.h"
#include "MeshGpuComponent.h"
#include "MaterialGpuComponent.h"
#include "RenderDirtyComponent.h"
#include "RenderGpuComponent.h"
#include "GeometryBindingLifecycle.h"

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
            if (!ctx.world.hasComponent<QuadMeshComponent>(entity)
                || !ctx.world.hasComponent<MaterialComponent>(entity))
            {
                continue;
            }

            gts::rendering::syncQuadMeshBinding(ctx.world, entity, ctx.resources, commands);
        }
    }
};
