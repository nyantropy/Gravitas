#pragma once

#include <vector>
#include <string>
#include "GlmConfig.h"

enum class TileType : uint8_t
{
    Wall        = 0,
    Floor       = 1,
    StairDown   = 2,
    StairUp     = 3,
    Treasure    = 4,
    EnemySpawn  = 5,
};

struct GeneratedRoom
{
    int  x       = 0;
    int  y       = 0;
    int  width   = 0;
    int  height  = 0;
    bool isCustom = false;
};

struct GeneratedFloor
{
    int  floorNumber = 0;
    int  width       = 0;
    int  height      = 0;
    std::vector<TileType>      tiles;
    std::vector<GeneratedRoom> rooms;
    std::vector<glm::vec3>     enemySpawnPositions;
    std::vector<glm::ivec2>    treasureSpawns;
    glm::ivec2                 stairDownPos = {-1, -1};
    glm::ivec2                 stairUpPos   = {-1, -1};
    glm::ivec2                 playerStart = {1, 1};

    TileType get(int x, int z) const       { return tiles[z * width + x]; }
    void     set(int x, int z, TileType t) { tiles[z * width + x] = t; }

    bool inBounds(int x, int z) const
    {
        return x >= 0 && x < width && z >= 0 && z < height;
    }

    bool isWalkable(int x, int z) const
    {
        if (!inBounds(x, z)) return false;
        return get(x, z) != TileType::Wall;
    }

    bool hasStairUp() const
    {
        return stairUpPos.x >= 0 && stairUpPos.y >= 0;
    }

    bool hasStairDown() const
    {
        return stairDownPos.x >= 0 && stairDownPos.y >= 0;
    }
};
