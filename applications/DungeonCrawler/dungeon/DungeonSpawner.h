#pragma once

#include <vector>

#include "Entity.h"

#include "dungeon/generator/GeneratedFloor.h"

class ECSWorld;

class DungeonSpawner
{
public:
    void buildFloor(ECSWorld& world,
                    std::vector<Entity>& floorEntities,
                    const GeneratedFloor& floor) const;
    void destroyFloor(ECSWorld& world, std::vector<Entity>& floorEntities) const;

private:
    void spawnStairFeature(ECSWorld& world,
                           std::vector<Entity>& floorEntities,
                           const GeneratedFloor& floor,
                           const glm::ivec2& stairPos,
                           bool descends) const;
};
