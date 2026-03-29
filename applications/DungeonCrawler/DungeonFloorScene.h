#pragma once

#include "GlmConfig.h"
#include "GtsScene.hpp"
#include "SceneContext.h"
#include "GtsSceneTransitionData.h"
#include "Entity.h"
#include "Types.h"

#include "GeneratedFloor.h"

// Forward declarations (full types included in .cpp)
class DungeonFloorScene : public GtsScene
{
public:
    explicit DungeonFloorScene(GeneratedFloor floor);

    void onLoad(SceneContext& ctx,
                const GtsSceneTransitionData* data = nullptr) override;

    void onUpdateSimulation(SceneContext& ctx) override;
    void onUpdateControllers(SceneContext& ctx) override;

    bool enableWalls   = false;
    bool enableCeiling = false;

private:
    GeneratedFloor generatedFloor;

    Entity playerEntity   = Entity{ std::numeric_limits<entity_id_type>::max() };
    Entity debugCamEntity = Entity{ std::numeric_limits<entity_id_type>::max() };

    void spawnFloorTiles(SceneContext& ctx);
    void spawnEnemies(SceneContext& ctx);
    void spawnPlayer(SceneContext& ctx, glm::ivec2 startPos);
    void spawnDebugCamera(SceneContext& ctx);

    std::vector<glm::ivec2> generatePatrolPath(glm::ivec2 start, int length) const;
};
