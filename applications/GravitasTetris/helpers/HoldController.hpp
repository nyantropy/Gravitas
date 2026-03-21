#pragma once

#include "TetrominoType.hpp"
#include "TetrominoShape.hpp"
#include "TetrisBlockComponent.hpp"
#include "HeldPieceBlockComponent.hpp"
#include "HierarchyHelper.h"
#include "ECSWorld.hpp"
#include "Entity.h"
#include <array>
#include "GlmConfig.h"

// Owns the hold slot state and the four persistent hold-display ECS entities.
struct HoldController
{
    // Fixed world position of the hold-display anchor (left sidebar).
    static constexpr glm::ivec2 HOLD_DISPLAY_PIVOT = { -5, 16 };

    struct HoldResult
    {
        bool          performed;      // false: hold was blocked (usedThisTurn)
        bool          hasSwappedIn;   // true: place newActiveType; false: spawn from queue
        TetrominoType newActiveType;  // valid only when hasSwappedIn is true
    };

    explicit HoldController(Entity holdAnchor) : holdAnchor(holdAnchor) {}

    // Creates the four display entities parented to the hold anchor.
    // Blocks start off-screen; they become visible on the first successful hold.
    void init(ECSWorld& world)
    {
        for (int i = 0; i < 4; ++i)
        {
            Entity e = world.createEntity();
            world.addComponent(e, TetrisBlockComponent{ -100, -100, true, TetrominoType::I });
            world.addComponent(e, HeldPieceBlockComponent{});
            displayBlocks[i] = e;
            setParent(world, e, holdAnchor);
        }
    }

    // Validates and performs the hold action; updates internal state on success.
    HoldResult tryHold(TetrominoType activeType)
    {
        if (state.usedThisTurn) return { false, false, {} };

        state.usedThisTurn = true;

        if (state.hasHeld)
        {
            TetrominoType prev = state.heldType;
            state.heldType     = activeType;
            return { true, true, prev };
        }

        state.heldType = activeType;
        state.hasHeld  = true;
        return { true, false, {} };
    }

    // Resets the per-piece hold guard so the next piece can use hold.
    void resetTurn() { state.usedThisTurn = false; }

    bool          hasHeld()     const { return state.hasHeld; }
    TetrominoType heldType()    const { return state.heldType; }
    bool          isAvailable() const { return !state.usedThisTurn; }

    // Repositions the display blocks to reflect the current hold state.
    void refreshDisplay(ECSWorld& world)
    {
        if (!state.hasHeld)
        {
            for (int i = 0; i < 4; ++i)
            {
                auto& b = world.getComponent<TetrisBlockComponent>(displayBlocks[i]);
                b.x = -100;
                b.y = -100;
            }
            return;
        }

        auto& shape = TetrominoShapes[(int)state.heldType][0];
        for (int i = 0; i < 4; ++i)
        {
            auto& b = world.getComponent<TetrisBlockComponent>(displayBlocks[i]);
            b.x    = shape.blocks[i].x;
            b.y    = shape.blocks[i].y;
            b.type = state.heldType;
        }
    }

private:
    struct HoldState
    {
        TetrominoType heldType     = TetrominoType::I;
        bool          hasHeld      = false;
        bool          usedThisTurn = false;
    };

    HoldState             state;
    std::array<Entity, 4> displayBlocks;
    Entity                holdAnchor;
};
