#pragma once

// 1 = wall (blocked), 0 = floor (walkable)
// 7x7 room with border walls and a few interior walls
static constexpr int MAP_W = 7;
static constexpr int MAP_H = 7;
static constexpr int DUNGEON_MAP[MAP_H][MAP_W] = {
    {1, 1, 1, 1, 1, 1, 1},
    {1, 0, 0, 0, 0, 0, 1},
    {1, 0, 1, 0, 0, 0, 1},
    {1, 0, 0, 0, 1, 0, 1},
    {1, 0, 0, 0, 0, 0, 1},
    {1, 0, 0, 0, 0, 0, 1},
    {1, 1, 1, 1, 1, 1, 1},
};

static bool isWalkable(int x, int z)
{
    if (x < 0 || x >= MAP_W || z < 0 || z >= MAP_H) return false;
    return DUNGEON_MAP[z][x] == 0;
}
