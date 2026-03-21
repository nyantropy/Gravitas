#pragma once
#include "ECSControllerSystem.hpp"
#include "TetrisBlockComponent.hpp"
#include "GhostBlockComponent.hpp"
#include "HeldPieceBlockComponent.hpp"
#include "NextPieceBlockComponent.hpp"
#include "TransformComponent.h"
#include "RenderDescriptionComponent.h"
#include "TetrominoType.hpp"
#include "GraphicsConstants.h"
#include "BoundsComponent.h"
#include "ECSWorld.hpp"
#include "SceneContext.h"

// First-pass controller system: attaches TransformComponent and
// RenderDescriptionComponent to any TetrisBlockComponent entity that
// does not yet have them.
//
// Must be registered before installRendererFeature() so it always runs
// before RenderBindingSystem. This guarantees that every block entity is
// fully described before the binding system inspects it — eliminating the
// one-frame texture flash that occurred when binding ran before the lazy-
// attach branch in TetrisVisualSystem.
class TetrisBlockInitSystem : public ECSControllerSystem
{
public:
    static constexpr float GHOST_ALPHA = 0.25f;

    void update(ECSWorld& world, SceneContext&) override
    {
        world.forEach<TetrisBlockComponent>([&](Entity e, TetrisBlockComponent& block)
        {
            if (world.hasComponent<TransformComponent>(e))
                return;  // already initialised

            const bool isGhost = world.hasComponent<GhostBlockComponent>(e);

            world.addComponent(e, TransformComponent{});
            world.addComponent(e, BoundsComponent{});  // unit cube local bounds for frustum culling

            RenderDescriptionComponent desc;
            desc.meshPath    = GraphicsConstants::ENGINE_RESOURCES + "/models/cube.obj";
            desc.texturePath = texturePath(block.type);
            if (isGhost) desc.alpha = GHOST_ALPHA;
            world.addComponent(e, desc);
        });
    }

private:
    static std::string texturePath(TetrominoType type)
    {
        switch (type)
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
