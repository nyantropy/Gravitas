#include "DungeonFloorDebug.h"

#include <iostream>
#include <sstream>

std::string buildDungeonFloorAscii(const GeneratedFloor& floor)
{
    std::ostringstream stream;

    for (int y = 0; y < floor.height; ++y)
    {
        for (int x = 0; x < floor.width; ++x)
        {
            char symbol = '#';
            if (floor.playerStart.x == x && floor.playerStart.y == y)
            {
                symbol = 'P';
            }
            else if (floor.hasStairUp() && floor.stairUpPos.x == x && floor.stairUpPos.y == y)
            {
                symbol = 'U';
            }
            else if (floor.hasStairDown() && floor.stairDownPos.x == x && floor.stairDownPos.y == y)
            {
                symbol = 'D';
            }
            else
            {
                switch (floor.get(x, y))
                {
                    case TileType::Wall:       symbol = '#'; break;
                    case TileType::Floor:      symbol = '.'; break;
                    case TileType::StairDown:  symbol = 'D'; break;
                    case TileType::StairUp:    symbol = 'U'; break;
                    case TileType::Treasure:   symbol = 'T'; break;
                    case TileType::EnemySpawn: symbol = 'E'; break;
                }
            }

            stream << symbol;
        }

        if (y + 1 < floor.height) stream << '\n';
    }

    return stream.str();
}

std::string buildDungeonFloorSummary(const GeneratedFloor& floor)
{
    std::ostringstream stream;
    stream << "floor=" << floor.floorNumber
           << ", size=" << floor.width << "x" << floor.height
           << ", rooms=" << floor.rooms.size()
           << ", enemies=" << floor.enemySpawns.size()
           << ", treasures=" << floor.treasureSpawns.size()
           << ", stairUp=" << (floor.hasStairUp() ? "yes" : "no")
           << ", stairDown=" << (floor.hasStairDown() ? "yes" : "no");
    return stream.str();
}

void dumpDungeonFloorAscii(const GeneratedFloor& floor, std::ostream& out)
{
    out << buildDungeonFloorAscii(floor) << '\n'
        << buildDungeonFloorSummary(floor) << '\n';
}
