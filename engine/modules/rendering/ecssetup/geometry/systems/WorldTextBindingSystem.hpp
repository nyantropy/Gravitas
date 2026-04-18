#pragma once

#include "ECSControllerSystem.hpp"
#include "WorldTextComponent.h"
#include "MeshGpuComponent.h"
#include "MaterialGpuComponent.h"
#include "RenderDirtyComponent.h"
#include "RenderGpuComponent.h"
#include "TransformComponent.h"
#include "GlyphLayoutEngine.h"
#include "Vertex.h"
#include "ECSWorld.hpp"
#include "GeometryBindingLifecycle.h"

class WorldTextBindingSystem : public ECSControllerSystem
{
public:
    void update(const EcsControllerContext& ctx) override
    {
        auto& commands = ctx.world.commands();
        ctx.world.forEachSnapshot<WorldTextComponent, TransformComponent>(
            [&](Entity e, WorldTextComponent& wtc, TransformComponent&)
        {
            if (!wtc.font || wtc.text.empty())
            {
                if (ctx.world.hasComponent<RenderGpuComponent>(e))
                    gts::rendering::scheduleRenderableCleanup(ctx.world, commands, e);
                return;
            }

            const bool hasMeshGpu = ctx.world.hasComponent<MeshGpuComponent>(e);
            const bool hasMatGpu = ctx.world.hasComponent<MaterialGpuComponent>(e);
            const bool hasRenderGpu = ctx.world.hasComponent<RenderGpuComponent>(e);
            const bool hasDirty = ctx.world.hasComponent<RenderDirtyComponent>(e);

            MeshGpuComponent meshGpu = hasMeshGpu ? ctx.world.getComponent<MeshGpuComponent>(e) : MeshGpuComponent{};
            MaterialGpuComponent matGpu = hasMatGpu ? ctx.world.getComponent<MaterialGpuComponent>(e) : MaterialGpuComponent{};
            RenderGpuComponent rc = hasRenderGpu ? ctx.world.getComponent<RenderGpuComponent>(e) : RenderGpuComponent{};
            RenderDirtyComponent dirty = hasDirty ? ctx.world.getComponent<RenderDirtyComponent>(e) : RenderDirtyComponent{};

            if (rc.objectSSBOSlot == RENDERABLE_SLOT_UNALLOCATED)
            {
                rc.objectSSBOSlot = ctx.resources->requestObjectSlot();
                rc.commandDirty = true;
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
                    dirty.meshDirty    = true;
                    dirty.materialDirty = true;
                    rc.dirty           = true;
                    rc.readyToRender   = false;
                    rc.commandDirty    = true;
                }

                wtc.dirty = false;
            }

            if (hasMeshGpu)
                ctx.world.getComponent<MeshGpuComponent>(e) = meshGpu;
            else
                commands.addComponent<MeshGpuComponent>(e, meshGpu);

            if (hasMatGpu)
                ctx.world.getComponent<MaterialGpuComponent>(e) = matGpu;
            else
                commands.addComponent<MaterialGpuComponent>(e, matGpu);

            if (hasRenderGpu)
                ctx.world.getComponent<RenderGpuComponent>(e) = rc;
            else
                commands.addComponent<RenderGpuComponent>(e, rc);

            if (hasDirty)
                ctx.world.getComponent<RenderDirtyComponent>(e) = dirty;
            else
                commands.addComponent<RenderDirtyComponent>(e, dirty);
        });
    }
};
