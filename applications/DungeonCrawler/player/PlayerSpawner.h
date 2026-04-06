#pragma once

#include <vector>

#include "Entity.h"
#include "GlmConfig.h"

#include "inventory/Item.h"

class ECSWorld;
struct SceneContext;

class PlayerSpawner
{
public:
    Entity spawnPlayer(ECSWorld& world,
                       SceneContext& ctx,
                       const glm::ivec2& startPos,
                       const std::vector<Item>& inventoryItems,
                       uint32_t goldAmount) const;
};
