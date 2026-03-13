#pragma once

#include <vector>
#include <cstdint>
#include "AiTypes.h"

// Singleton ECS component that holds all AI runtime state.
// Created disabled so the game starts in normal player-controlled mode.
struct AiComponent
{
    bool                   enabled                = false;
    bool                   needsReplan            = false;
    std::vector<MoveToken> moveQueue;

    // Identity of the active-piece entity observed last frame.
    // Changes when a new piece spawns — used to trigger replanning.
    uint32_t lastActivePieceEntityId = UINT32_MAX;

    // Active-piece state observed at the end of the previous controller frame.
    // Used to detect whether TetrisGameSystem acted on the front move token.
    int lastKnownPivotX   = 0;
    int lastKnownPivotY   = 0;
    int lastKnownRotation = 0;
};
