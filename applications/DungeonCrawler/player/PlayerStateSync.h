#pragma once

#include <vector>

#include "Entity.h"
#include "GlmConfig.h"

#include "inventory/Item.h"

class ECSWorld;

class PlayerStateSync
{
public:
    void movePlayerToTile(ECSWorld& world, Entity playerEntity, const glm::ivec2& gridPos) const;
    void syncPersistentState(ECSWorld& world,
                             Entity playerEntity,
                             std::vector<Item>& inventoryItems,
                             uint32_t& goldAmount) const;
};
