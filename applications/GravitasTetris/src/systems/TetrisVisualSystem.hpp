#pragma once

#include "ECSControllerSystem.hpp"
#include "components/TetrisBlockComponent.hpp"
#include "components/GhostBlockComponent.hpp"
#include "components/HeldPieceBlockComponent.hpp"
#include "components/NextPieceBlockComponent.hpp"
#include "TransformComponent.h"
#include "StaticMeshComponent.h"
#include "MaterialComponent.h"
#include "ECSWorld.hpp"
#include "SceneContext.h"
#include "struct/TetrominoType.hpp"
#include "helpers/TetrisPaths.hpp"
#include <string>

// Visual system: owns only the gameplay-level description of what a block looks like.
// Attaches StaticMeshComponent + MaterialComponent (paths only) and keeps
// TransformComponent in sync with block grid coordinates.
// No resource manager calls; all GPU binding is handled by StaticMeshBindingSystem.
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
                const bool isNext  = world.hasComponent<NextPieceBlockComponent>(e);

                // TetrisBlockInitSystem guarantees TransformComponent, StaticMeshComponent,
                // and MaterialComponent are attached before this system runs.
                // Keep texture in sync for blocks whose type can change at runtime.
                if (isGhost || isHeld || isNext)
                {
                    auto& mat           = world.getComponent<MaterialComponent>(e);
                    std::string newPath = texturePath(block);

                    if (newPath != mat.texturePath)
                        mat.texturePath = newPath;
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
                else if (isNext)
                {
                    // Preview blocks are in local space relative to the next anchor.
                    // Center each piece horizontally under the NEXT label (anchor local x = 0).
                    glm::vec2 co = previewCenteringOffset(block.type);
                    tr.position.x = float(block.x) + co.x;
                    tr.position.y = float(block.y);
                }
                else
                {
                    tr.position.x = float(block.x);
                    tr.position.y = float(block.y);
                }
                tr.position.z = 0.0f;
            });
        }

    private:
        // Returns the x-center of a piece type's rotation-0 bounding box.
        // Shared by holdCenteringOffset and previewCenteringOffset.
        //   I  — x:[-1..2] → center 0.5
        //   O  — x:[0..1]  → center 0.5
        //   T/S/Z/J/L — x:[-1..1] → center 0.0
        static float pieceXCenter(TetrominoType type)
        {
            switch (type)
            {
                case TetrominoType::I:
                case TetrominoType::O: return 0.5f;
                default:               return 0.0f;
            }
        }

        // Returns the float offset (in hold-anchor local space) that moves the
        // rotation-0 bounding box of the given piece type to the center of the hold
        // box.  The hold box interior local center is (0.5, 0.5).
        //   I  — y:[0]   (center 0) → shift (0,    +0.5)
        //   O  — y:[0..1](center 0.5) → shift (0,    0)   [already centered]
        //   T/S/Z/J/L — y:[0..1](center 0.5) → shift (+0.5, 0)
        static glm::vec2 holdCenteringOffset(TetrominoType type)
        {
            const float xShift = 0.5f - pieceXCenter(type);
            const float yShift = (type == TetrominoType::I) ? 0.5f : 0.0f;
            return { xShift, yShift };
        }

        // Returns the float x offset (in next-anchor local space) that horizontally
        // centers the rotation-0 bounding box under the NEXT label.
        // The anchor local center x is 0.0 (NEXT label spans [-2, 2], center = 0).
        static glm::vec2 previewCenteringOffset(TetrominoType type)
        {
            return { 0.0f - pieceXCenter(type), 0.0f };
        }

        std::string texturePath(const TetrisBlockComponent& block)
        {
            return gtsTetrisTexturePath(block.type);
        }
};
