#pragma once

#include "GtsScene.hpp"
#include "SceneContext.h"
#include "GtsSceneTransitionData.h"
#include "Entity.h"
#include "Types.h"

#include "GeneratedFloor.h"

#include <limits>

// Handcrafted two-floor scene for testing FloorTransitionSystem.
// Layout: Floor A (10×10, Y=0, X 0..9) | Ramp (4 tiles wide, Y 0→-2) | Floor B (10×10, Y=-2, X 14..23)
//
// Controls:
//   WASD   — move          Q/E — turn
//   T      — toggle debug bird's-eye camera
class StairTestScene : public GtsScene
{
public:
    void onLoad(SceneContext& ctx,
                const GtsSceneTransitionData* data = nullptr) override;

    void onUpdateSimulation(SceneContext& ctx) override;
    void onUpdateControllers(SceneContext& ctx) override;

private:
    GeneratedFloor testFloors[2];
    Entity         playerEntity   = Entity{std::numeric_limits<entity_id_type>::max()};
    Entity         debugCamEntity = Entity{std::numeric_limits<entity_id_type>::max()};

    void buildTestFloors();
    void spawnFloor(int floorIdx);
    void spawnRamp();
    void spawnPlayer(SceneContext& ctx);
    void spawnDebugCamera(SceneContext& ctx);
};
