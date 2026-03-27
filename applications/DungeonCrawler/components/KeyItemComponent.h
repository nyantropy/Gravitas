#pragma once

// Marks the dungeon key item entity.
// CombatSystem destroys this entity when the player steps onto its grid cell.
struct KeyItemComponent
{
    int gridX = 0;
    int gridZ = 0;
};
