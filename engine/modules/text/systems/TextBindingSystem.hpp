#pragma once

#include "ECSControllerSystem.hpp"
#include "TextComponent.h"
#include "TextMeshComponent.h"
#include "RenderGpuComponent.h"
#include "TransformComponent.h"

// Controller system that bridges TextMeshComponent (CPU geometry) with the
// normal renderable pipeline (RenderGpuComponent → RenderCommandExtractor).
//
// Responsibilities (mirrors RenderBindingSystem but for text entities):
//   - Creates RenderGpuComponent on first encounter, seeding the model matrix
//     from TransformComponent immediately so there is no first-frame identity flash.
//   - Allocates an SSBO slot (once, like RenderBindingSystem does).
//   - Sets textureID directly from font.atlasTexture — no path resolution needed.
//   - When TextMeshComponent::gpuDirty: uploads the vertex/index data to a GPU
//     mesh via IResourceProvider::uploadProceduralMesh and stores the returned ID.
//   - Sets RenderGpuComponent::dirty so RenderGpuSystem re-uploads the model matrix.
//
// Text entities do NOT carry RenderDescriptionComponent, so RenderBindingSystem
// leaves them entirely alone.  After this system runs, RenderCommandExtractor
// treats them identically to any other renderable entity.
class TextBindingSystem : public ECSControllerSystem
{
    public:
        void update(ECSWorld& world, SceneContext& ctx) override
        {
            world.forEach<TextComponent, TextMeshComponent>(
                [&](Entity e, TextComponent& text, TextMeshComponent& tm)
            {
                if (!text.font)
                    return;

                // --- Ensure RenderGpuComponent exists ---
                if (!world.hasComponent<RenderGpuComponent>(e))
                {
                    RenderGpuComponent rc{};

                    // Seed the model matrix now so the entity is correctly
                    // positioned on the very first frame without waiting for
                    // RenderGpuSystem to run in the next simulation tick.
                    if (world.hasComponent<TransformComponent>(e))
                    {
                        rc.modelMatrix = world.getComponent<TransformComponent>(e).getModelMatrix();
                        rc.dirty = false;
                    }

                    world.addComponent(e, rc);
                }

                auto& rc = world.getComponent<RenderGpuComponent>(e);

                // --- Allocate SSBO slot once ---
                if (rc.objectSSBOSlot == RENDERABLE_SLOT_UNALLOCATED)
                    rc.objectSSBOSlot = ctx.resources->requestObjectSlot();

                // --- Bind atlas texture (update each frame in case font pointer changes) ---
                rc.textureID = text.font->atlasTexture;

                // --- Upload geometry when the CPU mesh was rebuilt ---
                if (tm.gpuDirty && !tm.vertices.empty())
                {
                    tm.meshId = ctx.resources->uploadProceduralMesh(
                        tm.meshId, tm.vertices, tm.indices);

                    rc.meshID = tm.meshId;
                    rc.dirty  = true;   // trigger RenderGpuSystem model-matrix re-upload
                    tm.gpuDirty = false;
                }
            });
        }
};
