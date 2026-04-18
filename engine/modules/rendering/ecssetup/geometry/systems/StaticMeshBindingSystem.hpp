#pragma once

#include "ECSControllerSystem.hpp"
#include "StaticMeshComponent.h"
#include "MaterialComponent.h"
#include "MeshGpuComponent.h"
#include "MaterialGpuComponent.h"
#include "RenderDirtyComponent.h"
#include "RenderGpuComponent.h"
#include "GeometryBindingLifecycle.h"

// Explicit lifecycle pass for static mesh renderables.
// Reads StaticMeshComponent + MaterialComponent and is the only system permitted to call
// ctx.resources->requestMesh / requestTexture / requestObjectSlot for static entities.
//
// Responsibilities:
//   - Creates MeshGpuComponent, MaterialGpuComponent, RenderGpuComponent on first frame
//   - Allocates an SSBO slot on first bind
//   - Resolves mesh/texture paths to GPU IDs on first bind and whenever they change
//   - Sets dirty state so RenderGpuSystem re-uploads the model matrix
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
            if (!ctx.world.hasComponent<StaticMeshComponent>(entity)
                || !ctx.world.hasComponent<MaterialComponent>(entity))
            {
                continue;
            }

            gts::rendering::syncStaticMeshBinding(ctx.world, entity, ctx.resources, commands);
        }
    }
};
