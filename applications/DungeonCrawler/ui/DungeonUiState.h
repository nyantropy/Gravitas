#pragma once

#include "GlmConfig.h"

#include "dungeon/DungeonVisibilityState.h"
#include "dungeon/generator/GeneratedFloor.h"

struct DungeonUiState
{
    int                        currentFloorIndex = 0;
    int                        totalFloorCount = 0;
    bool                       debugCameraActive = false;
    MinimapRevealMode          minimapRevealMode = MinimapRevealMode::ExploredOnly;
    glm::ivec2                 playerTile = {0, 0};
    const GeneratedFloor*      activeFloor = nullptr;
    const DungeonVisibilityState* visibility = nullptr;
};
