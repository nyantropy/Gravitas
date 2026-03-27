#pragma once

#include "ECSControllerSystem.hpp"
#include "WorldImageComponent.h"
#include "RenderGpuComponent.h"
#include "Vertex.h"

// Controller system that allocates and updates GPU resources for entities
// carrying a WorldImageComponent.  Mirrors RenderBindingSystem but generates
// a procedural flat quad mesh instead of loading one from disk.
//
// The quad is axis-aligned in the XY plane, centered at the entity's origin.
// Apply rotation via TransformComponent to face the camera as needed.
class WorldImageBindingSystem : public ECSControllerSystem
{
public:
    void update(ECSWorld& world, SceneContext& ctx) override
    {
        world.forEach<WorldImageComponent>([&](Entity e, WorldImageComponent& img)
        {
            if (img.texturePath.empty())
                return;

            // Ensure the GPU binding component exists
            if (!world.hasComponent<RenderGpuComponent>(e))
                world.addComponent(e, RenderGpuComponent{});

            auto& rc = world.getComponent<RenderGpuComponent>(e);

            // Allocate an SSBO slot on first bind
            if (rc.objectSSBOSlot == RENDERABLE_SLOT_UNALLOCATED)
                rc.objectSSBOSlot = ctx.resources->requestObjectSlot();

            // World image quads are single-polygon planes — always visible from both sides.
            rc.doubleSided = true;

            // Resolve (or re-resolve) texture if the path changed
            if (img.texturePath != img._boundTexturePath)
            {
                rc.textureID           = ctx.resources->requestTexture(img.texturePath);
                img._boundTexturePath  = img.texturePath;
                rc.dirty               = true;
            }

            // Re-upload the procedural quad mesh whenever dimensions change
            if (img.width != img._boundWidth || img.height != img._boundHeight)
            {
                const float hw = img.width  * 0.5f;
                const float hh = img.height * 0.5f;

                std::vector<Vertex> verts = {
                    { { -hw,  hh, 0.0f }, { 1.0f, 1.0f, 1.0f }, { 0.0f, 0.0f } },
                    { {  hw,  hh, 0.0f }, { 1.0f, 1.0f, 1.0f }, { 1.0f, 0.0f } },
                    { {  hw, -hh, 0.0f }, { 1.0f, 1.0f, 1.0f }, { 1.0f, 1.0f } },
                    { { -hw, -hh, 0.0f }, { 1.0f, 1.0f, 1.0f }, { 0.0f, 1.0f } },
                };
                std::vector<uint32_t> idxs = { 0, 1, 2, 0, 2, 3 };

                rc.meshID        = ctx.resources->uploadProceduralMesh(rc.meshID, verts, idxs);
                img._boundWidth  = img.width;
                img._boundHeight = img.height;
                rc.dirty         = true;
            }
        });
    }
};
