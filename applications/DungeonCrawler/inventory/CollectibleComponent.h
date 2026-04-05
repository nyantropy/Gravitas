#pragma once

#include <cstdint>

#include "GlmConfig.h"

#include "inventory/CollectibleType.h"
#include "inventory/Item.h"

struct CollectibleComponent
{
    CollectibleType type = CollectibleType::InventoryItem;
    Item            item;
    uint32_t        goldAmount = 0;
    int             floorIndex = 0;
    glm::ivec2      gridPosition = {0, 0};
    float           rotationSpeed = 1.8f;
};
