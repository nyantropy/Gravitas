#pragma once

#include <vector>
#include "ECSControllerSystem.hpp"
#include "WorldTextComponent.h"
#include "WorldTextRuntimeComponent.h"
#include "BoundsComponent.h"
#include "MaterialReferenceComponent.h"
#include "MeshGpuComponent.h"
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
                cleanupTextMesh(ctx.world, commands, e, runtime, wtc);
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
                cleanupTextMesh(ctx.world, commands, e, runtime, wtc);
                return;
            }

            const bool hasMeshGpu = ctx.world.hasComponent<MeshGpuComponent>(e);

            MeshGpuComponent meshGpu = hasMeshGpu ? ctx.world.getComponent<MeshGpuComponent>(e) : MeshGpuComponent{};

            const bool layoutChanged = textLayoutChanged(wtc, runtime);
            const bool materialChanged =
                !ctx.world.hasComponent<MaterialReferenceComponent>(e)
                || !runtime.initialized
                || runtime.lastTint != wtc.tint
                || runtime.lastFontPath != wtc.fontPath;
            const bool needsMeshUpload =
                layoutChanged
                || !hasMeshGpu
                || meshGpu.meshID == 0
                || !meshGpu.ownsProceduralMeshResource;

            if (needsMeshUpload)
            {
                std::vector<Vertex> verts;
                std::vector<uint32_t> indices;
                GlyphLayoutEngine::build(wtc, *font, verts, indices);

                if (verts.empty() || indices.empty())
                {
                    cleanupTextMesh(ctx.world, commands, e, runtime, wtc);
                    return;
                }

                const mesh_id_type existingId = meshGpu.ownsProceduralMeshResource ? meshGpu.meshID : 0;
                meshGpu.meshID = ctx.resources->uploadProceduralMesh(
                    existingId,
                    verts,
                    indices,
                    StandardVertexAttributes);
                meshGpu.ownsProceduralMeshResource = true;
                meshGpu.boundMeshPath = {};
                meshGpu.boundQuadWidth = 0.0f;
                meshGpu.boundQuadHeight = 0.0f;
                meshGpu.boundDynamicGeometryVersion = 0;
                updateBounds(ctx.world, e, verts);

                gts::rendering::markMeshRepresentationDirty(ctx.world, e);
                gts::rendering::queueRenderObjectRefresh(ctx.world, e);
            }

            if (materialChanged)
                gts::rendering::queueMaterialRefresh(ctx.world, e);
            cacheDescriptor(runtime, wtc);

            if (hasMeshGpu)
                ctx.world.getComponent<MeshGpuComponent>(e) = meshGpu;
            else
                commands.addComponent<MeshGpuComponent>(e, meshGpu);

            if (needsMeshUpload)
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

    static void cleanupTextMesh(ECSWorld& world,
                                ECSWorld::EntityCommandBuffer& commands,
                                Entity entity,
                                WorldTextRuntimeComponent& runtime,
                                const WorldTextComponent& text)
    {
        gts::rendering::scheduleMeshGpuCleanup(world, commands, entity);
        gts::rendering::queueRenderableCleanup(world, entity);
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
