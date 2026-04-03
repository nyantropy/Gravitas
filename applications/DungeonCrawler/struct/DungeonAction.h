#pragma once

// DungeonCrawler-specific input actions.
// Kept here rather than in GtsAction to avoid polluting the engine-level
// enum with application concerns. Pass this enum as the template argument
// to InputActionManager<DungeonAction> inside DungeonInputSystem.
enum class DungeonAction
{
    MoveForward = 0,
    MoveBackward,
    StrafeLeft,
    StrafeRight,
    TurnLeft,           // Q — rotate facing 90° counter-clockwise
    TurnRight,          // E — rotate facing 90° clockwise
    Attack,             // Space — strike the cell directly ahead
    RegenerateDungeon,  // V — regenerate the full dungeon run

    // sentinel — always keep last
    ACTION_COUNT
};
