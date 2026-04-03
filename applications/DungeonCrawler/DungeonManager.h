#pragma once

#include <string>
#include <vector>
#include <cstdint>

#include "generator/DungeonSpec.h"
#include "generator/DungeonGenerator.h"

// Runtime coordinator for a single dungeon run.
// Owns the master seed, generates all floors in order, and carries the active
// floor index for gameplay systems and scene-building code.
class DungeonManager
{
public:
    explicit DungeonManager(uint32_t masterSeed = 0x12345u, int totalFloors = 4)
        : masterSeed(masterSeed)
        , totalFloorCount(totalFloors)
    {
    }

    void generateRun();

    int getTotalFloorCount() const { return totalFloorCount; }
    int getCurrentFloorIndex() const { return currentFloorIndex; }
    void setCurrentFloorIndex(int floorIndex);

    const GeneratedFloor& getActiveFloor() const;
    GeneratedFloor&       getActiveFloor();
    const GeneratedFloor* getFloor(int floorIndex) const;
    GeneratedFloor*       getFloor(int floorIndex);
    const std::vector<GeneratedFloor>& getFloors() const { return floors; }

    bool canMoveToFloor(int floorIndex) const;
    bool moveToFloor(int floorIndex);
    bool moveUp();
    bool moveDown();

    static DungeonSpec makeSpec(int floorNumber, int totalFloorCount, uint32_t floorSeed)
    {
        DungeonSpec spec;
        spec.floorNumber = floorNumber;
        spec.minRoomSize = 5;
        spec.maxRoomSize = 14;
        spec.seed        = floorSeed;
        spec.placeStairUp   = floorNumber > 0;
        spec.placeStairDown = floorNumber + 1 < totalFloorCount;

        switch (floorNumber)
        {
            case 0:
                spec.width  = 40;
                spec.height = 40;
                spec.budget.roomCost      = 20;
                spec.budget.enemyCount    = 4;
                spec.budget.treasureCount = 3;
                break;

            case 1:
                spec.width  = 50;
                spec.height = 40;
                spec.budget.roomCost      = 25;
                spec.budget.enemyCount    = 6;
                spec.budget.treasureCount = 4;
                break;

            case 2:
                spec.width  = 50;
                spec.height = 50;
                spec.budget.roomCost      = 30;
                spec.budget.enemyCount    = 8;
                spec.budget.treasureCount = 5;
                break;

            case 3:
                spec.width  = 60;
                spec.height = 50;
                spec.budget.roomCost      = 35;
                spec.budget.enemyCount    = 10;
                spec.budget.treasureCount = 6;
                break;

            default:
                break;
        }

        // Three room templates shared across all floors
        // ── Vendor room 8x8 cost=10: all floor tiles ──────────────────────
        {
            RoomTemplate vendor;
            vendor.name   = "vendor_room";
            vendor.width  = 8;
            vendor.height = 8;
            vendor.cost   = 10;
            vendor.tiles.assign(static_cast<size_t>(8 * 8), 0); // all floor
            spec.templates.push_back(vendor);
        }

        // ── Treasure vault 6x6 cost=8: all treasure tiles ─────────────────
        {
            RoomTemplate vault;
            vault.name   = "treasure_vault";
            vault.width  = 6;
            vault.height = 6;
            vault.cost   = 8;
            vault.tiles.assign(static_cast<size_t>(6 * 6), 4); // all treasure
            spec.templates.push_back(vault);
        }

        // ── Ambush room 10x6 cost=6: floor with enemy_spawn at both ends ──
        {
            RoomTemplate ambush;
            ambush.name   = "ambush_room";
            ambush.width  = 10;
            ambush.height = 6;
            ambush.cost   = 6;
            ambush.tiles.assign(static_cast<size_t>(10 * 6), 0); // floor
            // Place enemy_spawn (value 5) at column 0 and column 9 of row 3
            const int midRow = 3;
            ambush.tiles[midRow * 10 + 0] = 5;
            ambush.tiles[midRow * 10 + 9] = 5;
            spec.templates.push_back(ambush);
        }

        return spec;
    }

private:
    static uint32_t mixSeed(uint32_t baseSeed, uint32_t value);

    uint32_t                     masterSeed      = 0;
    int                          totalFloorCount = 4;
    int                          currentFloorIndex = 0;
    DungeonGenerator generator;
    std::vector<GeneratedFloor>  floors;
};
