#pragma once

#include <vector>
#include "ECSControllerSystem.hpp"
#include "WorldTextComponent.h"
#include "WorldTextRuntimeComponent.h"
#include "BoundsComponent.h"
#include "MeshGpuComponent.h"
#include "MaterialGpuComponent.h"
#include "RenderDirtyComponent.h"
#include "RenderGpuComponent.h"
#include "TransformComponent.h"
#include "GlyphLayoutEngine.h"
#include "Vertex.h"
#include "ECSWorld.hpp"
#include "GeometryBindingLifecycle.h"
#include "TransformDirtyHelpers.h"

class WorldTextBindingSystem : public ECSControllerSystem
{
public:
    void update(const EcsControllerContext& ctx) override
    {
        if (ctx.resources == nullptr)
            return;

        auto& commands = ctx.world.commands();
        ctx.world.forEach<WorldTextComponent, WorldTextRuntimeComponent, TransformComponent>(
            [&](Entity e,
                WorldTextComponent& wtc,
                WorldTextRuntimeComponent& runtime,
                TransformComponent&)
        {
            if (wtc.text.empty() || wtc.fontPath.empty())
            {
                cleanupRenderable(ctx.world, commands, e, runtime, wtc);
                return;
            }

            if (runtime.fontID == 0 || runtime.boundFontPath != wtc.fontPath)
            {
                runtime.fontID = ctx.resources->requestFont(wtc.fontPath);
                runtime.boundFontPath = wtc.fontPath;
            }

            const BitmapFont* font = ctx.resources->getFont(runtime.fontID);
            if (font == nullptr)
            {
                cleanupRenderable(ctx.world, commands, e, runtime, wtc);
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
            bool renderStateChanged = false;

            const bool layoutChanged = textLayoutChanged(wtc, runtime);
            const bool needsMeshUpload =
                layoutChanged
                || !hasMeshGpu
                || meshGpu.meshID == 0
                || !meshGpu.ownsProceduralMeshResource;
            const bool materialChanged =
                matGpu.textureID != font->atlasTexture
                || matGpu.tint != wtc.tint
                || matGpu.blendMode != MaterialBlendMode::Alpha
                || !matGpu.doubleSided
                || matGpu.vertexColorOnly
                || !matGpu.depthWrite
                || !runtime.initialized
                || runtime.lastTint != wtc.tint;

            if (needsMeshUpload)
            {
                std::vector<Vertex> verts;
                std::vector<uint32_t> indices;
                GlyphLayoutEngine::build(wtc, *font, verts, indices);

                if (verts.empty() || indices.empty())
                {
                    cleanupRenderable(ctx.world, commands, e, runtime, wtc);
                    return;
                }

                const mesh_id_type existingId = meshGpu.ownsProceduralMeshResource ? meshGpu.meshID : 0;
                meshGpu.meshID = ctx.resources->uploadProceduralMesh(existingId, verts, indices);
                meshGpu.ownsProceduralMeshResource = true;
                meshGpu.boundMeshPath = {};
                meshGpu.boundQuadWidth = 0.0f;
                meshGpu.boundQuadHeight = 0.0f;
                meshGpu.boundDynamicGeometryVersion = 0;
                dirty.meshDirty = true;
                gts::rendering::markRenderableDirty(dirty, rc);
                updateBounds(ctx.world, e, verts);
                renderStateChanged = true;
            }

            if (rc.objectSSBOSlot == RENDERABLE_SLOT_UNALLOCATED)
            {
                rc.objectSSBOSlot = ctx.resources->requestObjectSlot();
                rc.commandDirty = true;
                renderStateChanged = true;
            }

            if (materialChanged)
            {
                dirty.materialDirty = true;
                dirty.objectDataDirty = true;
                rc.commandDirty = true;
                renderStateChanged = true;
            }

            matGpu.textureID = font->atlasTexture;
            matGpu.tint = wtc.tint;
            matGpu.blendMode = MaterialBlendMode::Alpha;
            matGpu.doubleSided = true;
            matGpu.vertexColorOnly = false;
            matGpu.depthWrite = true;
            matGpu.boundTexturePath = wtc.fontPath;
            cacheDescriptor(runtime, wtc);

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

            if (renderStateChanged)
                gts::transform::markDirty(ctx.world, e);
        });
    }

private:
    static bool textLayoutChanged(const WorldTextComponent& text,
                                  const WorldTextRuntimeComponent& runtime)
    {
        return !runtime.initialized
            || runtime.lastText != text.text
            || runtime.lastFontPath != text.fontPath
            || runtime.lastScale != text.scale;
    }

    static void cacheDescriptor(WorldTextRuntimeComponent& runtime,
                                const WorldTextComponent& text)
    {
        runtime.lastText = text.text;
        runtime.lastFontPath = text.fontPath;
        runtime.lastScale = text.scale;
        runtime.lastTint = text.tint;
        runtime.initialized = true;
    }

    static void cleanupRenderable(ECSWorld& world,
                                  ECSWorld::EntityCommandBuffer& commands,
                                  Entity entity,
                                  WorldTextRuntimeComponent& runtime,
                                  const WorldTextComponent& text)
    {
        gts::rendering::scheduleRenderableCleanup(world, commands, entity);
        runtime.lastText = text.text;
        runtime.lastFontPath = text.fontPath;
        runtime.lastScale = text.scale;
        runtime.lastTint = text.tint;
        runtime.initialized = true;
    }

    static void updateBounds(ECSWorld& world,
                             Entity entity,
                             const std::vector<Vertex>& verts)
    {
        if (!world.hasComponent<BoundsComponent>(entity) || verts.empty())
            return;

        glm::vec3 min = verts[0].pos;
        glm::vec3 max = verts[0].pos;
        for (const Vertex& vertex : verts)
        {
            min = glm::min(min, vertex.pos);
            max = glm::max(max, vertex.pos);
        }

        BoundsComponent& bounds = world.getComponent<BoundsComponent>(entity);
        bounds.min = min;
        bounds.max = max;
    }
};
