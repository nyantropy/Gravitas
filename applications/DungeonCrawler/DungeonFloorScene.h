#pragma once

#include "GlmConfig.h"
#include "GtsScene.hpp"
#include "SceneContext.h"
#include "GtsSceneTransitionData.h"
#include "Entity.h"
#include "Types.h"
#include "BitmapFont.h"
#include "UiHandle.h"

#include "GeneratedFloor.h"
#include "generator/DungeonGenerator.h"
#include "DungeonConstants.h"

#include <vector>
#include <limits>

// Single-scene dungeon that owns all floors simultaneously, each offset on the
// Y axis by FLOOR_Y_OFFSET units. The player physically moves (lerps) between
// floors when stepping on a stair tile.  No scene reload is needed.
class DungeonFloorScene : public GtsScene
{
public:
    // Generates all floors at construction time (deterministic, no GPU work).
    DungeonFloorScene();

    void onLoad(SceneContext& ctx,
                const GtsSceneTransitionData* data = nullptr) override;

    void onUpdateSimulation(SceneContext& ctx) override;
    void onUpdateControllers(SceneContext& ctx) override;

    bool enableWalls   = false;
    bool enableCeiling = false;

private:
    // All floors generated at construction; all spawned simultaneously in onLoad.
    DungeonGenerator            generator;
    std::vector<GeneratedFloor> floors;

    Entity playerEntity   = Entity{ std::numeric_limits<entity_id_type>::max() };
    Entity debugCamEntity = Entity{ std::numeric_limits<entity_id_type>::max() };

    // Floor indicator UI — lifetime must exceed the UiTree's reference to font
    BitmapFont floorFont;
    UiHandle   floorIndicatorHandle = UI_INVALID_HANDLE;
    int        lastDisplayedFloor   = 0;

    // Spawners — per-floor versions take a GeneratedFloor parameter
    void spawnFloorTiles(SceneContext& ctx, const GeneratedFloor& floor);
    void spawnEnemies(SceneContext& ctx, const GeneratedFloor& floor);
    void spawnRamps(SceneContext& ctx, const GeneratedFloor& floor);

    // Player and debug camera persist across floor transitions
    void spawnPlayer(SceneContext& ctx, glm::ivec2 startPos);
    void spawnDebugCamera(SceneContext& ctx);

    std::vector<glm::ivec2> generatePatrolPath(const GeneratedFloor& floor,
                                               glm::ivec2 start, int length) const;
};
