#pragma once

#include "GeneratedFloor.h"

// Singleton component that holds a pointer to the current floor's GeneratedFloor.
// Set in DungeonFloorScene::onLoad after the floor is fully constructed.
// Read by movement systems (PlayerMovementSystem, EnemyMovementSystem) to
// validate walkability against the procedurally-generated map.
struct DungeonFloorSingleton
{
    const GeneratedFloor* floor = nullptr;
};
