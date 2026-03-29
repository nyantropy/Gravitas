#pragma once

#include "GeneratedFloor.h"

// Marks an entity as a dungeon floor tile spawned by DungeonFloorScene.
struct DungeonTileComponent
{
    TileType tileType   = TileType::Floor;
    int      floorNumber = 0;
};
