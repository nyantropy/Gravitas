#pragma once

#include <vector>

#include "dungeon/generator/GeneratedFloor.h"

enum class MinimapRevealMode : uint8_t
{
    ExploredOnly = 0,
    FullReveal
};

struct DungeonFloorVisibility
{
    int               width = 0;
    int               height = 0;
    std::vector<bool> exploredTiles;
    std::vector<bool> revealedRooms;

    bool inBounds(int x, int z) const
    {
        return x >= 0 && x < width && z >= 0 && z < height;
    }

    size_t tileIndex(int x, int z) const
    {
        return static_cast<size_t>(z * width + x);
    }

    bool isExplored(int x, int z) const
    {
        return inBounds(x, z) && exploredTiles[tileIndex(x, z)];
    }

    void setExplored(int x, int z, bool explored = true)
    {
        if (!inBounds(x, z)) return;
        exploredTiles[tileIndex(x, z)] = explored;
    }
};

class DungeonVisibilityState
{
public:
    void initialize(const std::vector<GeneratedFloor>& floors);
    void clear();

    void setRevealMode(MinimapRevealMode mode) { revealMode = mode; }
    MinimapRevealMode getRevealMode() const { return revealMode; }
    void toggleRevealMode();

    void revealAt(int floorIndex, const GeneratedFloor& floor, int x, int z);
    bool isVisible(int floorIndex, int x, int z) const;

    const DungeonFloorVisibility* getFloorVisibility(int floorIndex) const;

private:
    int findRoomIndex(const GeneratedFloor& floor, int x, int z) const;
    void revealRoom(DungeonFloorVisibility& visibility, const GeneratedRoom& room);
    void revealCorridorTiles(DungeonFloorVisibility& visibility, const GeneratedFloor& floor, int x, int z);

    MinimapRevealMode                  revealMode = MinimapRevealMode::ExploredOnly;
    std::vector<DungeonFloorVisibility> floorVisibility;
};
