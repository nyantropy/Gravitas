#pragma once

#include "ActiveTetromino.hpp"
#include "TetrisGrid.hpp"
#include "TetrisBlockComponent.hpp"
#include "TetrominoShape.hpp"
#include "TetrominoType.hpp"
#include "TetrisPhysics.hpp"
#include "ECSWorld.hpp"
#include <optional>
#include "GlmConfig.h"

// Owns the active falling piece and all single-piece mutations:
// movement, rotation (SRS wall-kick), hard-drop snap, locking, and spawning.
struct ActivePieceController
{
    ActiveTetromino active;  // readable by orchestrator; mutate via methods below

    // ── spawn lifecycle ───────────────────────────────────────────────────────
    // Step 1: set piece type, rotation-0, and spawn pivot.  Resets lock delay.
    // Call isGrounded() after this to detect game-over before creating entities.
    void prepareSpawn(const TetrisGrid& grid, TetrominoType type)
    {
        active.type     = type;
        active.rotation = 0;
        active.pivot    = { grid.width / 2, grid.height };
        resetLockDelay();
    }

    // Step 2: create the four ECS block entities and write their positions.
    // Must be called after any game-over wipe so the new entities survive.
    void materializeBlocks(ECSWorld& world)
    {
        for (int i = 0; i < 4; ++i)
        {
            Entity e = world.createEntity();
            world.addComponent(e, TetrisBlockComponent{ 0, 0, true, active.type });
            active.blocks[i] = e;
        }
        applyToBlocks(world);
    }

    // Removes the current active block entities from ECS (used by hold, not lock).
    void clearFromECS(ECSWorld& world)
    {
        for (int i = 0; i < 4; ++i)
            world.destroyEntity(active.blocks[i]);
    }

    // ── transform ────────────────────────────────────────────────────────────
    void applyToBlocks(ECSWorld& world)
    {
        auto& shape = TetrominoShapes[(int)active.type][active.rotation];
        for (int i = 0; i < 4; ++i)
        {
            auto& b    = world.getComponent<TetrisBlockComponent>(active.blocks[i]);
            glm::ivec2 p = active.pivot + shape.blocks[i];
            b.x = p.x;
            b.y = p.y;
        }
    }

    bool tryMove(ECSWorld& world, const TetrisGrid& grid, glm::ivec2 delta)
    {
        glm::ivec2 newPivot = active.pivot + delta;
        if (TetrisPhysics::testPosition(grid, active.type, newPivot, active.rotation))
        {
            active.pivot = newPivot;
            applyToBlocks(world);
            return true;
        }
        return false;
    }

    bool tryRotate(ECSWorld& world, const TetrisGrid& grid, int dir)
    {
        if (auto r = wallKick(grid, dir))
        {
            active.pivot    = r->pivot;
            active.rotation = r->rotation;
            applyToBlocks(world);
            return true;
        }
        return false;
    }

    void hardDropSnap(ECSWorld& world, const TetrisGrid& grid)
    {
        active.pivot = TetrisPhysics::computeDropPivot(grid, active);
        applyToBlocks(world);
    }

    // Marks active blocks as locked (no longer part of the falling piece).
    void lockPiece(ECSWorld& world)
    {
        for (int i = 0; i < 4; ++i)
            world.getComponent<TetrisBlockComponent>(active.blocks[i]).active = false;
    }

    // ── lock delay ───────────────────────────────────────────────────────────
    void  resetLockDelay()                    { lockDelay = 0.0f; }
    void  addLockDelay(float dt)              { lockDelay += dt; }
    bool  lockExpired(float limit) const      { return lockDelay >= limit; }

    // ── queries ──────────────────────────────────────────────────────────────
    // True when the piece cannot descend one row.  After prepareSpawn this also
    // serves as the game-over test (spawn position is blocked).
    bool isGrounded(const TetrisGrid& grid) const
    {
        return !TetrisPhysics::testPosition(
            grid, active.type,
            { active.pivot.x, active.pivot.y - 1 },
            active.rotation);
    }

private:
    float lockDelay = 0.0f;

    // ── SRS wall-kick tables (y-UP) ───────────────────────────────────────────
    struct KickOffset { int x; int y; };

    static constexpr KickOffset KICKS_JLSTZ_CW[4][5] = {{
        { 0, 0}, {-1, 0}, {-1,-1}, { 0, 2}, {-1, 2}
    },{
        { 0, 0}, { 1, 0}, { 1, 1}, { 0,-2}, { 1,-2}
    },{
        { 0, 0}, { 1, 0}, { 1,-1}, { 0, 2}, { 1, 2}
    },{
        { 0, 0}, {-1, 0}, {-1, 1}, { 0,-2}, {-1,-2}
    }};

    static constexpr KickOffset KICKS_JLSTZ_CCW[4][5] = {{
        { 0, 0}, { 1, 0}, { 1,-1}, { 0, 2}, { 1, 2}
    },{
        { 0, 0}, { 1, 0}, { 1, 1}, { 0,-2}, { 1,-2}
    },{
        { 0, 0}, {-1, 0}, {-1,-1}, { 0, 2}, {-1, 2}
    },{
        { 0, 0}, {-1, 0}, {-1, 1}, { 0,-2}, {-1,-2}
    }};

    static constexpr KickOffset KICKS_I_CW[4][5] = {{
        { 0, 0}, {-2, 0}, { 1, 0}, {-2, 1}, { 1,-2}
    },{
        { 0, 0}, {-1, 0}, { 2, 0}, {-1,-2}, { 2, 1}
    },{
        { 0, 0}, { 2, 0}, {-1, 0}, { 2,-1}, {-1, 2}
    },{
        { 0, 0}, { 1, 0}, {-2, 0}, { 1, 2}, {-2,-1}
    }};

    static constexpr KickOffset KICKS_I_CCW[4][5] = {{
        { 0, 0}, {-1, 0}, { 2, 0}, {-1,-2}, { 2, 1}
    },{
        { 0, 0}, { 2, 0}, {-1, 0}, { 2,-1}, {-1, 2}
    },{
        { 0, 0}, { 1, 0}, {-2, 0}, { 1, 2}, {-2,-1}
    },{
        { 0, 0}, {-2, 0}, { 1, 0}, {-2, 1}, { 1,-2}
    }};

    struct RotationResult { glm::ivec2 pivot; int rotation; };

    std::optional<RotationResult> wallKick(const TetrisGrid& grid, int direction) const
    {
        const int fromRot = active.rotation;
        const int toRot   = (fromRot + direction + 4) % 4;

        const KickOffset (*kicks)[5] =
            (active.type == TetrominoType::I)
                ? (direction > 0 ? KICKS_I_CW    : KICKS_I_CCW)
                : (direction > 0 ? KICKS_JLSTZ_CW : KICKS_JLSTZ_CCW);

        for (int k = 0; k < 5; ++k)
        {
            glm::ivec2 candidate = active.pivot +
                glm::ivec2(kicks[fromRot][k].x, kicks[fromRot][k].y);
            if (TetrisPhysics::testPosition(grid, active.type, candidate, toRot))
                return RotationResult{ candidate, toRot };
        }
        return std::nullopt;
    }
};
