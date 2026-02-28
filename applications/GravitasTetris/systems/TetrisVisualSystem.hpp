#pragma once

#include "ECSControllerSystem.hpp"
#include "TetrisBlockComponent.hpp"
#include "TransformComponent.h"
#include "RenderDescriptionComponent.h"
#include "RenderGpuComponent.h"
#include "ECSWorld.hpp"
#include "SceneContext.h"
#include "TetrominoType.hpp"
#include "GraphicsConstants.h"
#include <string>

// Visual system: owns only the gameplay-level description of what a block looks like.
// Attaches RenderDescriptionComponent (paths only) and keeps TransformComponent in sync
// with block grid coordinates.  No resource manager calls; all GPU binding is handled
// by RenderBindingSystem.
class TetrisVisualSystem : public ECSControllerSystem
{
    public:
        void update(ECSWorld& world, SceneContext&) override
        {
            world.forEach<TetrisBlockComponent>([&](Entity e, TetrisBlockComponent& block)
            {
                // on first encounter: attach description and transform
                if (!world.hasComponent<TransformComponent>(e))
                {
                    world.addComponent(e, TransformComponent{});

                    RenderDescriptionComponent desc;
                    desc.meshPath    = GraphicsConstants::ENGINE_RESOURCES + "/models/cube.obj";
                    desc.texturePath = texturePath(block);
                    world.addComponent(e, desc);
                }

                // keep the block's grid position reflected in the transform
                auto& tr = world.getComponent<TransformComponent>(e);
                tr.position.x = float(block.x);
                tr.position.y = float(block.y);
                tr.position.z = 0.0f;

                // flag the renderer-side component dirty so the model matrix is re-uploaded
                if (world.hasComponent<RenderGpuComponent>(e))
                    world.getComponent<RenderGpuComponent>(e).dirty = true;
            });
        }

    private:
        std::string texturePath(const TetrisBlockComponent& block)
        {
            switch(block.type)
            {
                case TetrominoType::I: return GraphicsConstants::ENGINE_RESOURCES + "/textures/cyan_texture.png";
                case TetrominoType::J: return GraphicsConstants::ENGINE_RESOURCES + "/textures/green_texture.png";
                case TetrominoType::L: return GraphicsConstants::ENGINE_RESOURCES + "/textures/yellow_texture.png";
                case TetrominoType::O: return GraphicsConstants::ENGINE_RESOURCES + "/textures/red_texture.png";
                case TetrominoType::S: return GraphicsConstants::ENGINE_RESOURCES + "/textures/purple_texture.png";
                case TetrominoType::T: return GraphicsConstants::ENGINE_RESOURCES + "/textures/orange_texture.png";
                case TetrominoType::Z: return GraphicsConstants::ENGINE_RESOURCES + "/textures/blue_texture.png";
                default:               return GraphicsConstants::ENGINE_RESOURCES + "/textures/grey_texture.png";
            }
        }
};
