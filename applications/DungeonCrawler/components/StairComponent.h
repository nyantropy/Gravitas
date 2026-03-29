#pragma once

enum class StairDirection
{
    Down,
    Up,
};

// Marks a tile entity as a staircase.
// StairInteractionSystem checks player grid position against stair positions
// and issues a ChangeScene command when the player steps on one.
struct StairComponent
{
    StairDirection direction   = StairDirection::Down;
    int            targetFloor = 0;
};
