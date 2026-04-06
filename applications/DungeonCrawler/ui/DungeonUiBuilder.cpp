#include "ui/DungeonUiBuilder.h"

#include <limits>

#include "DebugCameraStateComponent.h"
#include "components/PlayerComponent.h"
#include "ECSWorld.hpp"

DungeonUiState DungeonUiBuilder::buildState(const DungeonManager& dungeon,
                                            const DungeonVisibilityState& visibilityState,
                                            ECSWorld& world,
                                            Entity playerEntity) const
{
    constexpr Entity invalidEntity{std::numeric_limits<entity_id_type>::max()};

    DungeonUiState state;
    state.currentFloorIndex = dungeon.getCurrentFloorIndex();
    state.totalFloorCount = dungeon.getTotalFloorCount();
    state.minimapRevealMode = visibilityState.getRevealMode();
    state.activeFloor = &dungeon.getActiveFloor();
    state.visibility = &visibilityState;

    if (world.hasAny<DebugCameraStateComponent>())
        state.debugCameraActive = world.getSingleton<DebugCameraStateComponent>().active;

    if (playerEntity != invalidEntity)
    {
        const PlayerComponent& player = world.getComponent<PlayerComponent>(playerEntity);
        state.playerTile = {player.gridX, player.gridZ};
    }

    return state;
}
