#pragma once
#include <vector>
#include <cassert>
#include "Entity.h"

// a cache representation of a tetris grid, which is used by the game system
struct TetrisGrid
{
    int width = 10;
    int height = 20;

    std::vector<Entity> cells;

    TetrisGrid()
    {
        cells.resize(width * height, Entity{ UINT32_MAX });
    }

    bool inBounds(int x, int y) const
    {
        return x >= 0 && x < width && y >= 0 && y < height;
    }

    Entity& at(int x, int y)
    {
        assert(inBounds(x, y));
        return cells[y * width + x];
    }


    const Entity& at(int x, int y) const
    {
        assert(inBounds(x, y));
        return cells[y * width + x];
    }


    bool occupied(int x, int y) const
    {
        return at(x, y).id != UINT32_MAX;
    }
};
