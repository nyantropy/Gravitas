#pragma once

#include "ECSControllerSystem.hpp"
#include "WorldTextComponent.h"
#include "MeshGpuComponent.h"
#include "MaterialGpuComponent.h"
#include "RenderGpuComponent.h"
#include "TransformComponent.h"
#include "GlyphLayoutEngine.h"
#include "Vertex.h"
#include "ECSWorld.hpp"

class WorldTextBindingSystem : public ECSControllerSystem
{
public:
    void update(const EcsControllerContext& ctx) override
    {
        ctx.world.forEach<WorldTextComponent, TransformComponent>(
            [&](Entity e, WorldTextComponent& wtc, TransformComponent&)
        {
            if (!wtc.font || wtc.text.empty()) return;

            // Ensure all three GPU components exist
            if (!ctx.world.hasComponent<MeshGpuComponent>(e))
                ctx.world.addComponent(e, MeshGpuComponent{});
            if (!ctx.world.hasComponent<MaterialGpuComponent>(e))
                ctx.world.addComponent(e, MaterialGpuComponent{});
            if (!ctx.world.hasComponent<RenderGpuComponent>(e))
                ctx.world.addComponent(e, RenderGpuComponent{});

            auto& meshGpu = ctx.world.getComponent<MeshGpuComponent>(e);
            auto& matGpu  = ctx.world.getComponent<MaterialGpuComponent>(e);
            auto& rc      = ctx.world.getComponent<RenderGpuComponent>(e);

            if (rc.objectSSBOSlot == RENDERABLE_SLOT_UNALLOCATED)
            {
                rc.objectSSBOSlot = ctx.resources->requestObjectSlot();
                rc.commandDirty   = true;
            }

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
                    rc.commandDirty    = true;
                }

                wtc.dirty = false;
            }
        });
    }
};
