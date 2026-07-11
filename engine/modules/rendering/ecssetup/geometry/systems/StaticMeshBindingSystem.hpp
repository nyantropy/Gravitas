#pragma once

#include "ECSControllerSystem.hpp"
#include "GeometryBindingLifecycle.h"
#include "MeshBindingLifecycle.h"
#include "StaticMeshComponent.h"

// Explicit lifecycle pass for static mesh renderables.
// Reads StaticMeshComponent and owns only static mesh resource binding.
//
// Responsibilities:
//   - Creates or updates MeshGpuComponent
//   - Resolves mesh paths to GPU IDs on first bind and whenever they change
//   - Marks mesh representation dirty when the mesh resource changes
class StaticMeshBindingSystem : public ECSControllerSystem
{
public:
    void update(const EcsControllerContext& ctx) override
    {
        auto pendingStatic = gts::rendering::takeStaticMeshRefreshes(ctx.world);
        if (pendingStatic.empty())
            return;

        auto& commands = ctx.world.commands();

        for (entity_id_type entityId : pendingStatic)
        {
            Entity entity{entityId};
            if (!ctx.world.hasComponent<StaticMeshComponent>(entity))
            {
                continue;
            }

            gts::rendering::syncStaticMeshBinding(ctx.world, entity, ctx.resources, commands);
        }
    }
};
