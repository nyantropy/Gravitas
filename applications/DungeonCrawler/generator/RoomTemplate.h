#pragma once

#include <vector>
#include <string>

struct RoomTemplate
{
    std::string  name;
    int          width  = 0;
    int          height = 0;
    std::vector<int> tiles; // row-major: 0=floor,1=wall,2=stair_down,3=stair_up,4=treasure,5=enemy_spawn
    int          cost   = 5;
    bool         unique = false;
};
