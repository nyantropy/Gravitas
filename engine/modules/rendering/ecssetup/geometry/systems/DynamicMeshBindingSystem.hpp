#pragma once

#include "ECSControllerSystem.hpp"
#include "DynamicMeshComponent.h"
#include "MaterialComponent.h"
#include "MeshGpuComponent.h"
#include "MaterialGpuComponent.h"
#include "RenderDirtyComponent.h"
#include "RenderGpuComponent.h"
#include "GeometryBindingLifecycle.h"

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
            if (!ctx.world.hasComponent<DynamicMeshComponent>(entity)
                || !ctx.world.hasComponent<MaterialComponent>(entity))
            {
                continue;
            }

            gts::rendering::syncDynamicMeshBinding(ctx.world, entity, ctx.resources, commands);
        }
    }
};
