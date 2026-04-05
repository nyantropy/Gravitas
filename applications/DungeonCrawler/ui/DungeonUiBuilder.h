#pragma once

#include "Entity.h"

#include "dungeon/DungeonManager.h"
#include "dungeon/DungeonVisibilityState.h"
#include "ui/DungeonUiState.h"

class ECSWorld;

class DungeonUiBuilder
{
public:
    DungeonUiState buildState(const DungeonManager& dungeon,
                              const DungeonVisibilityState& visibilityState,
                              ECSWorld& world,
                              Entity playerEntity) const;
};
