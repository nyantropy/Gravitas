#pragma once

#include <functional>

#include "Entity.h"
#include "GlmConfig.h"

class ECSWorld;
class DungeonManager;
class DungeonVisibilityState;
struct SceneContext;

class DungeonTransitionSystem
{
public:
    using RebuildFloorFn = std::function<void()>;
    using MovePlayerFn = std::function<void(const glm::ivec2&)>;

    void reset();
    void update(SceneContext& ctx,
                ECSWorld& world,
                DungeonManager& dungeon,
                DungeonVisibilityState& visibilityState,
                Entity playerEntity,
                const RebuildFloorFn& rebuildFloor,
                const MovePlayerFn& movePlayerToTile);

private:
    bool stairLatchActive = false;
    int  latchedFloorIndex = -1;
    glm::ivec2 latchedStairPos = {-1, -1};

    void handleStairTransitions(SceneContext& ctx,
                                ECSWorld& world,
                                DungeonManager& dungeon,
                                DungeonVisibilityState& visibilityState,
                                Entity playerEntity,
                                const RebuildFloorFn& rebuildFloor,
                                const MovePlayerFn& movePlayerToTile);
    void beginFloorTransition(ECSWorld& world,
                              DungeonManager& dungeon,
                              Entity playerEntity,
                              bool movingDown,
                              const glm::ivec2& sourceGridPos);
    void updateFloorTransition(SceneContext& ctx,
                               ECSWorld& world,
                               DungeonManager& dungeon,
                               DungeonVisibilityState& visibilityState,
                               Entity playerEntity,
                               const RebuildFloorFn& rebuildFloor,
                               const MovePlayerFn& movePlayerToTile);
    void updateStairLatch(const DungeonManager& dungeon, const glm::ivec2& playerGridPos);
};
