#pragma once

#include "ECSControllerSystem.hpp"
#include "TetrisBlockComponent.hpp"
#include "GhostBlockComponent.hpp"
#include "HeldPieceBlockComponent.hpp"
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
        static constexpr float GHOST_ALPHA = 0.25f;

        void update(ECSWorld& world, SceneContext&) override
        {
            world.forEach<TetrisBlockComponent>([&](Entity e, TetrisBlockComponent& block)
            {
                const bool isGhost = world.hasComponent<GhostBlockComponent>(e);
                const bool isHeld  = world.hasComponent<HeldPieceBlockComponent>(e);

                // on first encounter: attach description and transform
                if (!world.hasComponent<TransformComponent>(e))
                {
                    world.addComponent(e, TransformComponent{});

                    RenderDescriptionComponent desc;
                    desc.meshPath    = GraphicsConstants::ENGINE_RESOURCES + "/models/cube.obj";
                    desc.texturePath = texturePath(block);
                    if (isGhost) desc.alpha = GHOST_ALPHA;
                    world.addComponent(e, desc);
                }
                else if (isGhost || isHeld)
                {
                    // Ghost and held blocks can change type (ghost: new piece spawns;
                    // held: piece swapped).  Keep texture in sync each frame so
                    // RenderBindingSystem detects the path change and rebinds.
                    auto& desc       = world.getComponent<RenderDescriptionComponent>(e);
                    desc.texturePath = texturePath(block);
                }

                // keep the block's grid position reflected in the transform
                auto& tr = world.getComponent<TransformComponent>(e);
                if (isHeld)
                {
                    // Held blocks are in local space relative to the hold anchor.
                    // Different piece types have different bounding-box centers so we
                    // apply a per-type sub-cell offset to visually center each piece
                    // inside the hold box (whose local center is at (0.5, 0.5)).
                    glm::vec2 co = holdCenteringOffset(block.type);
                    tr.position.x = float(block.x) + co.x;
                    tr.position.y = float(block.y) + co.y;
                }
                else
                {
                    tr.position.x = float(block.x);
                    tr.position.y = float(block.y);
                }
                tr.position.z = 0.0f;

                // flag the renderer-side component dirty so the model matrix is re-uploaded
                if (world.hasComponent<RenderGpuComponent>(e))
                    world.getComponent<RenderGpuComponent>(e).dirty = true;
            });
        }

    private:
        // Returns the float offset (in hold-anchor local space) that moves the
        // rotation-0 bounding box of the given piece type to the center of the hold
        // box.  The hold box interior local center is (0.5, 0.5).
        //
        // Derivation (rotation 0, blocks placed at shape.x / shape.y):
        //   I  — x:[-1..2] (center 0.5), y:[0]   (center  0) → shift (   0, +0.5)
        //   O  — x:[0..1]  (center 0.5), y:[0..1] (center 0.5) → shift (   0,    0)  [centered]
        //   T/S/Z/J/L — x:[-1..1] (center 0), y:[0..1] (center 0.5) → shift (+0.5,    0)
        static glm::vec2 holdCenteringOffset(TetrominoType type)
        {
            switch (type)
            {
                case TetrominoType::I: return { 0.0f, 0.5f };
                case TetrominoType::O: return { 0.0f, 0.0f };
                default:               return { 0.5f, 0.0f };
            }
        }

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
