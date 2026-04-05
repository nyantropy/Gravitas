#pragma once

#include <vector>

#include "Entity.h"
#include "GlmConfig.h"

#include "dungeon/DungeonManager.h"
#include "dungeon/generator/GeneratedFloor.h"

class ECSWorld;

class EnemySpawner
{
public:
    void spawnEnemies(ECSWorld& world,
                      std::vector<Entity>& floorEntities,
                      const GeneratedFloor& floor) const;
    void removeDefeatedEnemy(DungeonManager& dungeon,
                             int floorIndex,
                             const glm::ivec2& spawnTile) const;
};
