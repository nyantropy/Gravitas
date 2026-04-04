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
    void update(ECSWorld& world, SceneContext& ctx) override
    {
        world.forEach<ProceduralMeshComponent, MaterialComponent>(
            [&](Entity e, ProceduralMeshComponent& mesh, MaterialComponent& mat)
        {
            if (mat.texturePath.empty())
                return;

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

            // Allocate a GPU slot on first bind
            if (rc.objectSSBOSlot == RENDERABLE_SLOT_UNALLOCATED)
                rc.objectSSBOSlot = ctx.resources->requestObjectSlot();

            // Procedural quads are always visible from both sides
            matGpu.doubleSided = true;

            // Resolve (or re-resolve) texture if the path changed
            if (mat.texturePath != matGpu.boundTexturePath)
            {
                matGpu.textureID        = ctx.resources->requestTexture(mat.texturePath);
                matGpu.boundTexturePath = mat.texturePath;
                rc.dirty                = true;
                rc.readyToRender        = false;
            }

            matGpu.tint  = mat.tint;
            matGpu.alpha = mat.alpha;

            const bool customGeometryChanged =
                mesh.useCustomGeometry
                && mesh.geometryVersion != meshGpu.boundGeometryVersion;

            const bool quadGeometryChanged =
                !mesh.useCustomGeometry
                && (mesh.width != meshGpu.boundWidth || mesh.height != meshGpu.boundHeight);

            // Re-upload whenever the gameplay-facing procedural description changes.
            if (customGeometryChanged || quadGeometryChanged)
            {
                std::vector<Vertex> verts;
                std::vector<uint32_t> idxs;

                if (mesh.useCustomGeometry)
                {
                    verts = mesh.vertices;
                    idxs  = mesh.indices;
                }
                else
                {
                    const float hw = mesh.width  * 0.5f;
                    const float hh = mesh.height * 0.5f;

                    verts = {
                        { { -hw,  hh, 0.0f }, { 1.0f, 1.0f, 1.0f }, { 0.0f, 0.0f } },
                        { {  hw,  hh, 0.0f }, { 1.0f, 1.0f, 1.0f }, { 1.0f, 0.0f } },
                        { {  hw, -hh, 0.0f }, { 1.0f, 1.0f, 1.0f }, { 1.0f, 1.0f } },
                        { { -hw, -hh, 0.0f }, { 1.0f, 1.0f, 1.0f }, { 0.0f, 1.0f } },
                    };
                    idxs = { 0, 1, 2, 0, 2, 3 };
                }

                meshGpu.meshID      = ctx.resources->uploadProceduralMesh(meshGpu.meshID, verts, idxs);
                meshGpu.ownsProceduralMeshResource = true;
                meshGpu.boundWidth  = mesh.width;
                meshGpu.boundHeight = mesh.height;
                meshGpu.boundGeometryVersion = mesh.geometryVersion;
                rc.dirty            = true;
                rc.readyToRender    = false;
            }
        });
    }
};
