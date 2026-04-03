#pragma once

#include <iosfwd>
#include <string>

#include "GeneratedFloor.h"

std::string buildDungeonFloorAscii(const GeneratedFloor& floor);
std::string buildDungeonFloorSummary(const GeneratedFloor& floor);
void        dumpDungeonFloorAscii(const GeneratedFloor& floor, std::ostream& out);
