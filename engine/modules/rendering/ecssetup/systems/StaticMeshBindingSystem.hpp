#pragma once

#include "ECSControllerSystem.hpp"
#include "StaticMeshComponent.h"
#include "MaterialComponent.h"
#include "MeshGpuComponent.h"
#include "MaterialGpuComponent.h"
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
    void update(ECSWorld& world, SceneContext& ctx) override
    {
        world.forEach<StaticMeshComponent, MaterialComponent>(
            [&](Entity e, StaticMeshComponent& mesh, MaterialComponent& mat)
        {
            // Ensure GPU components exist
            if (!world.hasComponent<MeshGpuComponent>(e))
                world.addComponent(e, MeshGpuComponent{});
            if (!world.hasComponent<MaterialGpuComponent>(e))
                world.addComponent(e, MaterialGpuComponent{});
            if (!world.hasComponent<RenderGpuComponent>(e))
                world.addComponent(e, RenderGpuComponent{});

            auto& meshGpu = world.getComponent<MeshGpuComponent>(e);
            auto& matGpu  = world.getComponent<MaterialGpuComponent>(e);
            auto& rc      = world.getComponent<RenderGpuComponent>(e);

            // Allocate a GPU slot the first time this entity is seen
            if (rc.objectSSBOSlot == RENDERABLE_SLOT_UNALLOCATED)
                rc.objectSSBOSlot = ctx.resources->requestObjectSlot();

            // Resolve (or re-resolve) mesh if the path changed
            if (mesh.meshPath != meshGpu.boundMeshPath)
            {
                meshGpu.meshID        = ctx.resources->requestMesh(mesh.meshPath);
                meshGpu.ownsProceduralMeshResource = false;
                meshGpu.boundMeshPath = mesh.meshPath;
                rc.dirty              = true;
                rc.readyToRender      = false;
            }

            // Resolve (or re-resolve) texture if the path changed
            if (mat.texturePath != matGpu.boundTexturePath)
            {
                matGpu.textureID        = ctx.resources->requestTexture(mat.texturePath);
                matGpu.boundTexturePath = mat.texturePath;
                rc.dirty                = true;
                rc.readyToRender        = false;
            }

            // Sync material properties (cheap copies; no GPU resource involved)
            matGpu.tint        = mat.tint;
            matGpu.alpha       = mat.alpha;
            matGpu.doubleSided = mat.doubleSided;
        });
    }
};
