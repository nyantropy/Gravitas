#pragma once

#include "ECSControllerSystem.hpp"
#include "TetrisBlockComponent.hpp"
#include "TransformComponent.h"
#include "MeshComponent.h"
#include "ObjectGpuComponent.h"
#include "MaterialComponent.h"
#include "ECSWorld.hpp"
#include "SceneContext.h"
#include "TetrominoType.hpp"
#include <string>

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

                    ObjectGpuComponent ogc;
                    ogc.objectSSBOIndex = ctx.resources->requestObjectSlot();
                    ogc.dirty = true;
                    world.addComponent(e, ogc);

                    addMaterialComponent(world, ctx, block, e);
                }

                // apply block position to transform
                auto& tr = world.getComponent<TransformComponent>(e);
                tr.position.x = float(block.x);
                tr.position.y = float(block.y);
                tr.position.z = 0.0f;

                // mark dirty so ObjectGpuDataSystem uploads the updated model matrix
                world.getComponent<ObjectGpuComponent>(e).dirty = true;
            });
        }

        void addMaterialComponent(ECSWorld& world, SceneContext& ctx, TetrisBlockComponent& block, Entity& e)
        {
            std::string path = "";
            switch(block.type)
            {
                case TetrominoType::I:
                    path = GraphicsConstants::ENGINE_RESOURCES + "/textures/cyan_texture.png";
                    break;
                case TetrominoType::J:
                    path = GraphicsConstants::ENGINE_RESOURCES + "/textures/green_texture.png";
                    break;
                case TetrominoType::L:
                    path = GraphicsConstants::ENGINE_RESOURCES + "/textures/yellow_texture.png";
                    break;
                case TetrominoType::O:
                    path = GraphicsConstants::ENGINE_RESOURCES + "/textures/red_texture.png";
                    break;
                case TetrominoType::S:
                    path = GraphicsConstants::ENGINE_RESOURCES + "/textures/purple_texture.png";
                    break;
                case TetrominoType::T:
                    path = GraphicsConstants::ENGINE_RESOURCES + "/textures/orange_texture.png";
                    break;
                case TetrominoType::Z:
                    path = GraphicsConstants::ENGINE_RESOURCES + "/textures/blue_texture.png";
                    break;
                default:
                    path = GraphicsConstants::ENGINE_RESOURCES + "/textures/grey_texture.png";
                    break;
            }

            world.addComponent(e, MaterialComponent{ ctx.resources->requestTexture(path) });
        }
};
