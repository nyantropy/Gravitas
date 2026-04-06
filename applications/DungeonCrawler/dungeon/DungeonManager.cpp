#include "dungeon/DungeonManager.h"

#include <stdexcept>

namespace
{
    constexpr glm::ivec2 INVALID_GRID_COORD = {-1, -1};
}

uint32_t DungeonManager::mixSeed(uint32_t baseSeed, uint32_t value)
{
    baseSeed ^= value + 0x9E3779B9u + (baseSeed << 6) + (baseSeed >> 2);
    return baseSeed;
}

void DungeonManager::generateRun()
{
    floors.clear();
    floors.reserve(static_cast<size_t>(totalFloorCount));
    currentFloorIndex = 0;

    glm::ivec2 requiredUp = INVALID_GRID_COORD;
    for (int floorIndex = 0; floorIndex < totalFloorCount; ++floorIndex)
    {
        const uint32_t floorSeed = mixSeed(masterSeed, static_cast<uint32_t>(floorIndex));

        DungeonSpec spec        = makeSpec(floorIndex, totalFloorCount, floorSeed);
        spec.requiredStairUpPos = requiredUp;

        GeneratedFloor floor = generator.generate(spec);
        floors.push_back(std::move(floor));

        requiredUp = floors.back().hasStairDown()
            ? floors.back().stairDownPos
            : INVALID_GRID_COORD;
    }
}

void DungeonManager::setCurrentFloorIndex(int floorIndex)
{
    if (!canMoveToFloor(floorIndex))
        throw std::out_of_range("DungeonManager floor index out of range");

    currentFloorIndex = floorIndex;
}

const GeneratedFloor& DungeonManager::getActiveFloor() const
{
    return floors.at(static_cast<size_t>(currentFloorIndex));
}

GeneratedFloor& DungeonManager::getActiveFloor()
{
    return floors.at(static_cast<size_t>(currentFloorIndex));
}

const GeneratedFloor* DungeonManager::getFloor(int floorIndex) const
{
    if (!canMoveToFloor(floorIndex)) return nullptr;
    return &floors[static_cast<size_t>(floorIndex)];
}

GeneratedFloor* DungeonManager::getFloor(int floorIndex)
{
    if (!canMoveToFloor(floorIndex)) return nullptr;
    return &floors[static_cast<size_t>(floorIndex)];
}

bool DungeonManager::canMoveToFloor(int floorIndex) const
{
    return floorIndex >= 0 && floorIndex < static_cast<int>(floors.size());
}

bool DungeonManager::moveToFloor(int floorIndex)
{
    if (!canMoveToFloor(floorIndex)) return false;
    currentFloorIndex = floorIndex;
    return true;
}

bool DungeonManager::moveUp()
{
    return moveToFloor(currentFloorIndex - 1);
}

bool DungeonManager::moveDown()
{
    return moveToFloor(currentFloorIndex + 1);
}
