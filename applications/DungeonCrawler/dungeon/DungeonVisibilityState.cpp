#include "dungeon/DungeonVisibilityState.h"

namespace
{
    constexpr glm::ivec2 CARDINAL_DIRS[4] = {
        { 1,  0},
        {-1,  0},
        { 0,  1},
        { 0, -1}
    };
}

void DungeonVisibilityState::initialize(const std::vector<GeneratedFloor>& floors)
{
    floorVisibility.clear();
    floorVisibility.reserve(floors.size());

    for (const GeneratedFloor& floor : floors)
    {
        DungeonFloorVisibility visibility;
        visibility.width = floor.width;
        visibility.height = floor.height;
        visibility.exploredTiles.assign(static_cast<size_t>(floor.width * floor.height), false);
        visibility.revealedRooms.assign(floor.rooms.size(), false);
        floorVisibility.push_back(std::move(visibility));
    }
}

void DungeonVisibilityState::clear()
{
    floorVisibility.clear();
    revealMode = MinimapRevealMode::ExploredOnly;
}

void DungeonVisibilityState::toggleRevealMode()
{
    revealMode = (revealMode == MinimapRevealMode::ExploredOnly)
        ? MinimapRevealMode::FullReveal
        : MinimapRevealMode::ExploredOnly;
}

void DungeonVisibilityState::revealAt(int floorIndex, const GeneratedFloor& floor, int x, int z)
{
    if (floorIndex < 0 || floorIndex >= static_cast<int>(floorVisibility.size())) return;
    if (!floor.inBounds(x, z) || !floor.isWalkable(x, z)) return;

    DungeonFloorVisibility& visibility = floorVisibility[static_cast<size_t>(floorIndex)];

    const int roomIndex = findRoomIndex(floor, x, z);
    if (roomIndex >= 0)
    {
        if (!visibility.revealedRooms[static_cast<size_t>(roomIndex)])
            visibility.revealedRooms[static_cast<size_t>(roomIndex)] = true;

        revealRoom(visibility, floor.rooms[static_cast<size_t>(roomIndex)]);
        return;
    }

    revealCorridorTiles(visibility, floor, x, z);
}

bool DungeonVisibilityState::isVisible(int floorIndex, int x, int z) const
{
    if (revealMode == MinimapRevealMode::FullReveal) return true;

    const DungeonFloorVisibility* visibility = getFloorVisibility(floorIndex);
    return visibility && visibility->isExplored(x, z);
}

const DungeonFloorVisibility* DungeonVisibilityState::getFloorVisibility(int floorIndex) const
{
    if (floorIndex < 0 || floorIndex >= static_cast<int>(floorVisibility.size())) return nullptr;
    return &floorVisibility[static_cast<size_t>(floorIndex)];
}

int DungeonVisibilityState::findRoomIndex(const GeneratedFloor& floor, int x, int z) const
{
    for (size_t i = 0; i < floor.rooms.size(); ++i)
    {
        const GeneratedRoom& room = floor.rooms[i];
        if (x >= room.x && x < room.x + room.width
            && z >= room.y && z < room.y + room.height)
            return static_cast<int>(i);
    }

    return -1;
}

void DungeonVisibilityState::revealRoom(DungeonFloorVisibility& visibility, const GeneratedRoom& room)
{
    for (int z = room.y; z < room.y + room.height; ++z)
    {
        for (int x = room.x; x < room.x + room.width; ++x)
            visibility.setExplored(x, z, true);
    }
}

void DungeonVisibilityState::revealCorridorTiles(DungeonFloorVisibility& visibility,
                                                 const GeneratedFloor& floor,
                                                 int x,
                                                 int z)
{
    visibility.setExplored(x, z, true);

    for (const glm::ivec2& dir : CARDINAL_DIRS)
    {
        const int nx = x + dir.x;
        const int nz = z + dir.y;
        if (floor.inBounds(nx, nz) && floor.isWalkable(nx, nz))
            visibility.setExplored(nx, nz, true);
    }
}
