#pragma once

#include "GlmConfig.h"

#include "inventory/Item.h"

struct KeySpawnState
{
    Item       item;
    int        floorIndex = 0;
    glm::ivec2 gridPosition = {0, 0};
    bool       collected = false;
};
