#pragma once

// GravitasTetris-specific input actions.
// Kept here rather than in GtsAction to avoid polluting the engine-level
// enum with application concerns. Pass this enum as the template argument
// to InputActionManager<TetrisAction> inside TetrisInputSystem.
enum class TetrisAction
{
    MoveLeft = 0,
    MoveRight,
    RotateCW,    // E  — clockwise rotation
    RotateCCW,   // Q  — counter-clockwise rotation
    HardDrop,    // Space — instant drop
    SoftDrop,    // S  — accelerated drop

    // sentinel — always keep last
    ACTION_COUNT
};
