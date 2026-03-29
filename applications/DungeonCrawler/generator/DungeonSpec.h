#pragma once

#include <vector>
#include <cstdint>
#include "FloorBudget.h"
#include "RoomTemplate.h"

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
};
