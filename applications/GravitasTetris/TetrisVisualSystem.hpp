#pragma once

#include "ECSControllerSystem.hpp"
#include "TetrisBlockComponent.hpp"
#include "TransformComponent.h"
#include "MeshComponent.h"
#include "UniformBufferComponent.h"
#include "MaterialComponent.h"
#include "ECSWorld.hpp"
#include "SceneContext.h"

// the visual system is much more lightweight than the logical system, its responsibility lies only in making new tetris blocks renderable
// and adjusting the transformation coordinate inside the transform component
class TetrisVisualSystem : public ECSControllerSystem
{
    public:
        void update(ECSWorld& world, SceneContext& ctx) override
        {
            // update all blocks, active or static
            world.forEach<TetrisBlockComponent>([&](Entity e, TetrisBlockComponent& block)
            {
                // ensure render components exist
                if (!world.hasComponent<TransformComponent>(e))
                {
                    world.addComponent(e, TransformComponent{});
                    world.addComponent(e, MeshComponent{ ctx.resources->requestMesh(GraphicsConstants::ENGINE_RESOURCES + "/models/cube.obj") });
                    world.addComponent(e, UniformBufferComponent{ ctx.resources->requestUniformBuffer() });
                    world.addComponent(e, MaterialComponent{ ctx.resources->requestTexture(GraphicsConstants::ENGINE_RESOURCES + "/textures/green_texture.png") });
                }

                // apply block position to transform
                auto& tr = world.getComponent<TransformComponent>(e);
                tr.position.x = float(block.x);
                tr.position.y = float(block.y);
                tr.position.z = 0.0f;
            });
        }
};
