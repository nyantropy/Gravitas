#pragma once

#include "struct/TetrisGrid.hpp"
#include "components/TetrisBlockComponent.hpp"
#include "components/TetrisScoreComponent.hpp"
#include "components/NextPieceBlockComponent.hpp"
#include "components/GhostBlockComponent.hpp"
#include "components/HeldPieceBlockComponent.hpp"
#include "ECSWorld.hpp"
#include "Entity.h"
#include <vector>

// Owns the locked-block grid and all board-mutation operations:
// grid rebuilding, line clearing, and playfield wiping.
struct BoardController
{
    TetrisGrid grid;

    bool isDirty() const { return dirty; }
    void markDirty()     { dirty = true; }

    const TetrisGrid& getGrid() const { return grid; }

    // Rebuilds the spatial grid from current ECS state and clears the dirty flag.
    void rebuild(ECSWorld& world)
    {
        rebuildGrid(world);
        dirty = false;
    }

    // Scans for full rows, destroys their entities, shifts blocks above each
    // cleared row down, and emits a scoring event.  The grid must be current
    // before calling this.  A final rebuild after this call is the caller's
    // responsibility.
    void clearLines(ECSWorld& world)
    {
        int linesCleared = 0;

        for (int y = 0; y < grid.height; ++y)
        {
            bool full = true;
            for (int x = 0; x < grid.width; ++x)
                if (!grid.occupied(x, y)) { full = false; break; }

            if (!full) continue;
            linesCleared++;

            std::vector<Entity> toDestroy;
            for (int x = 0; x < grid.width; ++x)
            {
                Entity e = grid.at(x, y);
                if (e != Entity{ UINT32_MAX })
                    toDestroy.push_back(e);
            }
            for (Entity e : toDestroy)
                world.destroyEntity(e);

            // preview and ghost blocks have active=true, so the !b.active guard skips them
            world.forEach<TetrisBlockComponent>([&](Entity, TetrisBlockComponent& b)
            {
                if (!b.active && b.y > y) b.y -= 1;
            });

            y--;
            rebuildGrid(world);
        }

        if (linesCleared > 0)
        {
            auto& sc = world.getSingleton<TetrisScoreComponent>();
            sc.pendingEvents.push_back({ ScoringEventType::LinesCleared, linesCleared });
        }
    }

    // Destroys all playfield block entities (preserves ghost, next-preview, and held blocks).
    void wipePlayfield(ECSWorld& world)
    {
        std::vector<Entity> toWipe;
        world.forEach<TetrisBlockComponent>([&](Entity e, TetrisBlockComponent&)
        {
            if (!world.hasComponent<NextPieceBlockComponent>(e) &&
                !world.hasComponent<GhostBlockComponent>(e)    &&
                !world.hasComponent<HeldPieceBlockComponent>(e))
                toWipe.push_back(e);
        });
        for (Entity e : toWipe)
            world.destroyEntity(e);
    }

private:
    bool dirty = true;  // starts dirty to force the initial grid build

    void rebuildGrid(ECSWorld& world)
    {
        for (auto& c : grid.cells)
            c = Entity{ UINT32_MAX };

        world.forEach<TetrisBlockComponent>([&](Entity e, TetrisBlockComponent& b)
        {
            if (!grid.inBounds(b.x, b.y)) return;
            if (!b.active) grid.at(b.x, b.y) = e;
        });
    }
};
