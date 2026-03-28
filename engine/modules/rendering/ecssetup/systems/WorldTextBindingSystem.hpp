#pragma once

#include "ECSControllerSystem.hpp"
#include "WorldTextComponent.h"
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

            if (!world.hasComponent<RenderGpuComponent>(e))
                world.addComponent(e, RenderGpuComponent{});

            auto& rc = world.getComponent<RenderGpuComponent>(e);

            if (rc.objectSSBOSlot == RENDERABLE_SLOT_UNALLOCATED)
                rc.objectSSBOSlot = ctx.resources->requestObjectSlot();

            if (wtc.dirty)
            {
                std::vector<Vertex>   verts;
                std::vector<uint32_t> indices;
                GlyphLayoutEngine::build(wtc, verts, indices);

                if (!verts.empty())
                {
                    rc.meshID    = ctx.resources->uploadProceduralMesh(rc.meshID, verts, indices);
                    rc.textureID = wtc.font->atlasTexture;
                    rc.dirty     = true;
                }

                wtc.dirty = false;
            }
        });
    }
};
