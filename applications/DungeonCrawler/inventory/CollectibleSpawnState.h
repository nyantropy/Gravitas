#pragma once

#include <cstdint>

#include "GlmConfig.h"

#include "inventory/CollectibleType.h"
#include "inventory/Item.h"

struct CollectibleSpawnState
{
    CollectibleType type = CollectibleType::InventoryItem;
    Item            item;
    uint32_t        goldAmount = 0;
    int             floorIndex = 0;
    glm::ivec2      gridPosition = {0, 0};
    bool            collected = false;
};
