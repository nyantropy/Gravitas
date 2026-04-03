#pragma once

#include "GlmConfig.h"
#include "GeneratedFloor.h"

class DungeonManager;

// Singleton component that holds pointers to all generated floors and tracks
// which floor the player is currently on.
// Set in DungeonFloorScene::onLoad after all floors are constructed.
// Read by movement systems (PlayerMovementSystem, EnemyMovementSystem) to
// validate walkability against the procedurally-generated map.
//
// floorWorldOffset: world-space origin (bottom-left corner) of each floor.
// Used by PlayerMovementSystem and FloorTransitionSystem to convert local
// grid coordinates to world positions. x=X offset, y=base Y, z=Z offset.
struct DungeonFloorSingleton
{
    DungeonManager*       run                 = nullptr;
    const GeneratedFloor* floor               = nullptr; // active floor for walkability
    const GeneratedFloor* allFloors[4]        = {};      // all four floors
    int                   currentFloorIndex   = 0;
    glm::vec3             floorWorldOffset[4] = {};      // world origin per floor
};
