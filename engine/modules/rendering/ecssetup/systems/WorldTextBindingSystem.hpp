#pragma once

#include "ECSControllerSystem.hpp"
#include "WorldTextComponent.h"
#include "MeshGpuComponent.h"
#include "MaterialGpuComponent.h"
#include "RenderGpuComponent.h"
#include "TransformComponent.h"
#include "GlyphLayoutEngine.h"
#include "Vertex.h"
#include "SceneContext.h"
#include "ECSWorld.hpp"

class WorldTextBindingSystem : public ECSControllerSystem
{
public:
    void update(ECSWorld& world, SceneContext& ctx) override
    {
        world.forEach<WorldTextComponent, TransformComponent>(
            [&](Entity e, WorldTextComponent& wtc, TransformComponent&)
        {
            if (!wtc.font || wtc.text.empty()) return;

            // Ensure all three GPU components exist
            if (!world.hasComponent<MeshGpuComponent>(e))
                world.addComponent(e, MeshGpuComponent{});
            if (!world.hasComponent<MaterialGpuComponent>(e))
                world.addComponent(e, MaterialGpuComponent{});
            if (!world.hasComponent<RenderGpuComponent>(e))
                world.addComponent(e, RenderGpuComponent{});

            auto& meshGpu = world.getComponent<MeshGpuComponent>(e);
            auto& matGpu  = world.getComponent<MaterialGpuComponent>(e);
            auto& rc      = world.getComponent<RenderGpuComponent>(e);

            if (rc.objectSSBOSlot == RENDERABLE_SLOT_UNALLOCATED)
                rc.objectSSBOSlot = ctx.resources->requestObjectSlot();

            if (wtc.dirty)
            {
                std::vector<Vertex>   verts;
                std::vector<uint32_t> indices;
                GlyphLayoutEngine::build(wtc, verts, indices);

                if (!verts.empty())
                {
                    meshGpu.meshID     = ctx.resources->uploadProceduralMesh(meshGpu.meshID, verts, indices);
                    meshGpu.ownsProceduralMeshResource = true;
                    matGpu.textureID   = wtc.font->atlasTexture;
                    matGpu.doubleSided = true;
                    rc.dirty           = true;
                    rc.readyToRender   = false;
                }

                wtc.dirty = false;
            }
        });
    }
};
