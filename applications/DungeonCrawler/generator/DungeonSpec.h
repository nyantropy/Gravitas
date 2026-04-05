#pragma once

#include <vector>
#include <cstdint>
#include "generator/FloorBudget.h"
#include "generator/RoomTemplate.h"
#include "GlmConfig.h"

struct DungeonSpec
{
    int          floorNumber = 0;
    int          width       = 40;
    int          height      = 40;
    FloorBudget  budget;
    std::vector<RoomTemplate> templates;
    int          minRoomSize = 5;
    int          maxRoomSize = 14;
    uint32_t     seed        = 0;
    bool         placeStairUp   = false;
    bool         placeStairDown = false;

    // If x >= 0, the generator MUST place StairUp at this X/Z position.
    // Used by DungeonManager to align floor N+1's StairUp with floor N's StairDown.
    glm::ivec2 requiredStairUpPos = {-1, -1};
};
