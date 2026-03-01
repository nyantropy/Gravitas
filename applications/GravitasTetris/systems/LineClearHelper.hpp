#pragma once

#include "TetrisPhysicsHelper.hpp"
#include "TetrisBlockComponent.hpp"
#include "RenderResourceClearComponent.h"
#include "TetrisScoreComponent.hpp"
#include "Entity.h"
#include <vector>

// Scans the grid for full rows, destroys their ECS entities, shifts blocks above
// each cleared row down by one, and emits a scoring event.
//
// The caller must rebuild the grid before calling this function.
// The grid is rebuilt internally after each cleared row so multi-line clears are
// handled correctly.  The caller is responsible for a final rebuild after this
// function returns.
//
// Returns the number of lines cleared.
inline int clearLines(ECSWorld& world, TetrisGrid& grid)
{
    int linesCleared = 0;

    for (int y = 0; y < grid.height; ++y)
    {
        bool full = true;
        for (int x = 0; x < grid.width; ++x)
            if (!grid.occupied(x, y))
                full = false;

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
        {
            world.addComponent(e, RenderResourceClearComponent{});
            world.removeComponent<TetrisBlockComponent>(e);
        }

        // preview and ghost blocks have active=true, so the !b.active guard skips them
        world.forEach<TetrisBlockComponent>([&](Entity e, TetrisBlockComponent& b)
        {
            if (!b.active && b.y > y)
                b.y -= 1;
        });

        y--;
        rebuildGrid(world, grid);
    }

    if (linesCleared > 0)
    {
        auto& sc = world.getSingleton<TetrisScoreComponent>();
        sc.pendingEvents.push_back({ ScoringEventType::LinesCleared, linesCleared });
    }

    return linesCleared;
}
