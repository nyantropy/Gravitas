#pragma once

// Marks the dungeon exit tile entity.
// CombatSystem triggers a win when the player steps on this cell while hasKey == true.
struct ExitComponent
{
    int gridX = 0;
    int gridZ = 0;
};
