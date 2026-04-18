#pragma once

#include "ECSControllerSystem.hpp"
#include "ProceduralMeshComponent.h"
#include "MaterialComponent.h"
#include "MeshGpuComponent.h"
#include "MaterialGpuComponent.h"
#include "RenderDirtyComponent.h"
#include "RenderGpuComponent.h"
#include "Vertex.h"
#include "GeometryBindingLifecycle.h"

// Explicit lifecycle pass that allocates and updates GPU resources for entities
// carrying a ProceduralMeshComponent + MaterialComponent.
// Generates a procedural flat quad mesh instead of loading one from disk.
//
// The quad is axis-aligned in the XY plane, centered at the entity's origin.
// Apply rotation via TransformComponent to face the camera as needed.
class ProceduralMeshBindingSystem : public ECSControllerSystem
{
public:
    void update(const EcsControllerContext& ctx) override
    {
        auto pendingProcedural = gts::rendering::takeProceduralRefreshes(ctx.world);
        if (pendingProcedural.empty())
            return;

        auto& commands = ctx.world.commands();
        for (entity_id_type entityId : pendingProcedural)
        {
            Entity entity{entityId};
            if (!ctx.world.hasComponent<ProceduralMeshComponent>(entity)
                || !ctx.world.hasComponent<MaterialComponent>(entity))
            {
                continue;
            }

            gts::rendering::syncProceduralMeshBinding(ctx.world, entity, ctx.resources, commands);
        }
    }
};
