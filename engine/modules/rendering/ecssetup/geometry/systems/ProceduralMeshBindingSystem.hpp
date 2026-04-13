#pragma once

#include "ECSControllerSystem.hpp"
#include "ProceduralMeshComponent.h"
#include "MaterialComponent.h"
#include "MeshGpuComponent.h"
#include "MaterialGpuComponent.h"
#include "RenderDirtyComponent.h"
#include "RenderGpuComponent.h"
#include "Vertex.h"

// Controller system that allocates and updates GPU resources for entities
// carrying a ProceduralMeshComponent + MaterialComponent.
// Generates a procedural flat quad mesh instead of loading one from disk.
//
// The quad is axis-aligned in the XY plane, centered at the entity's origin.
// Apply rotation via TransformComponent to face the camera as needed.
class ProceduralMeshBindingSystem : public ECSControllerSystem
{
public:
    void update(const EcsControllerContext&) override
    {
        // Procedural mesh binding is lifecycle-driven via ECS add/remove callbacks.
    }
};
