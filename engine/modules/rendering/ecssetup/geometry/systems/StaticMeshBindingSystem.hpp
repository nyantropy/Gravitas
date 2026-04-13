#pragma once

#include "ECSControllerSystem.hpp"
#include "StaticMeshComponent.h"
#include "MaterialComponent.h"
#include "MeshGpuComponent.h"
#include "MaterialGpuComponent.h"
#include "RenderDirtyComponent.h"
#include "RenderGpuComponent.h"

// The exclusive broker between static mesh / material descriptions and renderer resources.
// Reads StaticMeshComponent + MaterialComponent and is the only system permitted to call
// ctx.resources->requestMesh / requestTexture / requestObjectSlot for static entities.
//
// Responsibilities:
//   - Creates MeshGpuComponent, MaterialGpuComponent, RenderGpuComponent on first frame
//   - Allocates an SSBO slot on first bind
//   - Resolves mesh/texture paths to GPU IDs on first bind and whenever they change
//   - Sets dirty = true so RenderGpuSystem re-uploads the model matrix
class StaticMeshBindingSystem : public ECSControllerSystem
{
public:
    void update(const EcsControllerContext&) override
    {
        // Static mesh binding is lifecycle-driven via ECS add/remove callbacks.
    }
};
