#pragma once

#include <vector>

#include "Entity.h"

#include "dungeon/DungeonManager.h"
#include "dungeon/generator/GeneratedFloor.h"
#include "inventory/CollectibleRunState.h"

class ECSWorld;

class CollectibleSpawner
{
public:
    void spawnCollectibles(ECSWorld& world,
                           std::vector<Entity>& floorEntities,
                           const GeneratedFloor& floor,
                           const CollectibleRunState& runState) const;
    void chooseNewCollectibleRunState(const DungeonManager& dungeon,
                                      CollectibleRunState& runState) const;
};
