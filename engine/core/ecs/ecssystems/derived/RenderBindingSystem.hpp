#pragma once

#include "ECSControllerSystem.hpp"
#include "RenderDescriptionComponent.h"
#include "RenderableComponent.h"

// The exclusive broker between gameplay intent and renderer resources.
// Reads RenderDescriptionComponent and is the only system permitted to call
// ctx.resources->requestMesh / requestTexture / requestObjectSlot.
//
// Responsibilities:
//   - Creates RenderableComponent on entities that have a description but no binding yet
//   - Allocates an SSBO slot on first bind
//   - Resolves mesh/texture paths to GPU IDs on first bind and whenever the paths change
//   - Sets dirty = true so RenderableTransformSystem re-uploads the model matrix
class RenderBindingSystem : public ECSControllerSystem
{
    public:
        void update(ECSWorld& world, SceneContext& ctx) override
        {
            world.forEach<RenderDescriptionComponent>([&](Entity e, RenderDescriptionComponent& desc)
            {
                // Ensure the internal binding component exists
                if (!world.hasComponent<RenderableComponent>(e))
                    world.addComponent(e, RenderableComponent{});

                auto& rc = world.getComponent<RenderableComponent>(e);

                // Allocate a GPU slot the first time this entity is seen
                if (rc.objectSSBOSlot == RENDERABLE_SLOT_UNALLOCATED)
                    rc.objectSSBOSlot = ctx.resources->requestObjectSlot();

                // Resolve (or re-resolve) mesh if the path changed
                if (desc.meshPath != rc.boundMeshPath)
                {
                    rc.meshID        = ctx.resources->requestMesh(desc.meshPath);
                    rc.boundMeshPath = desc.meshPath;
                    rc.dirty         = true;
                }

                // Resolve (or re-resolve) texture if the path changed
                if (desc.texturePath != rc.boundTexturePath)
                {
                    rc.textureID        = ctx.resources->requestTexture(desc.texturePath);
                    rc.boundTexturePath = desc.texturePath;
                    rc.dirty            = true;
                }
            });
        }
};
