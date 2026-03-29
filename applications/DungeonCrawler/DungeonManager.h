#pragma once

#include <memory>
#include <vector>
#include <string>

#include "GravitasEngine.hpp"
#include "DungeonFloorScene.h"
#include "generator/DungeonSpec.h"
#include "generator/DungeonGenerator.h"

// Configures and pre-generates all dungeon floors, then registers each as a
// named scene with the engine.  Call setup() before engine.setActiveScene().
//
// Floor naming convention: "floor_0", "floor_1", "floor_2", "floor_3"
class DungeonManager
{
public:
    void setup(GravitasEngine& engine)
    {
        for (int i = 0; i < 4; ++i)
        {
            DungeonSpec    spec  = makeSpec(i);
            GeneratedFloor floor = generator.generate(spec);

            engine.registerScene("floor_" + std::to_string(i),
                                  std::make_unique<DungeonFloorScene>(std::move(floor)));
        }
    }

    static DungeonSpec makeSpec(int floorNumber)
    {
        DungeonSpec spec;
        spec.floorNumber = floorNumber;
        spec.minRoomSize = 5;
        spec.maxRoomSize = 14;
        spec.seed        = 0; // deterministic per-floor seed via generator

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
    DungeonGenerator generator;
};
