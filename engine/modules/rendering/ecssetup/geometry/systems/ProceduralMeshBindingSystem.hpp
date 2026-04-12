#pragma once

#include "ECSControllerSystem.hpp"
#include "ProceduralMeshComponent.h"
#include "MaterialComponent.h"
#include "MeshGpuComponent.h"
#include "MaterialGpuComponent.h"
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
    void update(const EcsControllerContext& ctx) override
    {
        ctx.world.forEach<ProceduralMeshComponent, MaterialComponent>(
            [&](Entity e, ProceduralMeshComponent& mesh, MaterialComponent& mat)
        {
            if (mat.texturePath.empty())
                return;

            // Ensure GPU components exist
            if (!ctx.world.hasComponent<MeshGpuComponent>(e))
                ctx.world.addComponent(e, MeshGpuComponent{});
            if (!ctx.world.hasComponent<MaterialGpuComponent>(e))
                ctx.world.addComponent(e, MaterialGpuComponent{});
            if (!ctx.world.hasComponent<RenderGpuComponent>(e))
                ctx.world.addComponent(e, RenderGpuComponent{});

            auto& meshGpu = ctx.world.getComponent<MeshGpuComponent>(e);
            auto& matGpu  = ctx.world.getComponent<MaterialGpuComponent>(e);
            auto& rc      = ctx.world.getComponent<RenderGpuComponent>(e);

            // Allocate a GPU slot on first bind
            if (rc.objectSSBOSlot == RENDERABLE_SLOT_UNALLOCATED)
            {
                rc.objectSSBOSlot = ctx.resources->requestObjectSlot();
                rc.commandDirty   = true;
            }

            // Procedural quads are always visible from both sides
            matGpu.doubleSided = true;

            // Resolve (or re-resolve) texture if the path changed
            if (mat.texturePath != matGpu.boundTexturePath)
            {
                matGpu.textureID        = ctx.resources->requestTexture(mat.texturePath);
                matGpu.boundTexturePath = mat.texturePath;
                rc.dirty                = true;
                rc.readyToRender        = false;
                rc.commandDirty         = true;
            }

            if (matGpu.tint != mat.tint || matGpu.alpha != mat.alpha)
                rc.commandDirty = true;
            matGpu.tint  = mat.tint;
            matGpu.alpha = mat.alpha;

            const bool customGeometryChanged =
                mesh.useCustomGeometry
                && mesh.geometryVersion != meshGpu.boundGeometryVersion;

            const bool quadGeometryChanged =
                !mesh.useCustomGeometry
                && (mesh.width != meshGpu.boundWidth || mesh.height != meshGpu.boundHeight);

            // Re-upload/rebind whenever the gameplay-facing procedural description changes.
            if (customGeometryChanged || quadGeometryChanged)
            {
                if (mesh.useCustomGeometry)
                {
                    // Custom geometry is per-entity — upload directly.
                    std::vector<Vertex>   verts = mesh.vertices;
                    std::vector<uint32_t> idxs  = mesh.indices;
                    meshGpu.meshID = ctx.resources->uploadProceduralMesh(meshGpu.meshID, verts, idxs);
                    meshGpu.ownsProceduralMeshResource = true;
                }
                else
                {
                    // Standard axis-aligned quad: use a shared mesh so all same-size quads
                    // share one vertex/index buffer and can be instanced together.
                    if (meshGpu.ownsProceduralMeshResource && meshGpu.meshID != 0)
                        ctx.resources->releaseProceduralMesh(meshGpu.meshID);
                    meshGpu.meshID = ctx.resources->getSharedQuadMesh(mesh.width, mesh.height);
                    meshGpu.ownsProceduralMeshResource = false;
                }

                meshGpu.boundWidth           = mesh.width;
                meshGpu.boundHeight          = mesh.height;
                meshGpu.boundGeometryVersion = mesh.geometryVersion;
                rc.dirty                     = true;
                rc.readyToRender             = false;
                rc.commandDirty              = true;
            }
        });
    }
};
