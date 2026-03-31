#pragma once

#include "GtsScene.hpp"
#include "SceneContext.h"
#include "GtsSceneTransitionData.h"
#include "Entity.h"
#include "Types.h"

#include "GeneratedFloor.h"

#include <limits>

// Handcrafted two-floor scene for testing FloorTransitionSystem.
// No procedural generation — just two 10×10 rooms connected by stair triggers.
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
    // Floor 0 (Y=0): stair-down tile at (8,5)  — value 2
    // Floor 1 (Y=-4): stair-up tile at (1,5)   — value 3
    static constexpr int FLOOR0[10][10] = {
        {1,1,1,1,1,1,1,1,1,1},
        {1,0,0,0,0,0,0,0,0,1},
        {1,0,0,0,0,0,0,0,0,1},
        {1,0,0,0,0,0,0,0,0,1},
        {1,0,0,0,0,0,0,0,0,1},
        {1,0,0,0,0,0,0,0,2,1},
        {1,0,0,0,0,0,0,0,0,1},
        {1,0,0,0,0,0,0,0,0,1},
        {1,0,0,0,0,0,0,0,0,1},
        {1,1,1,1,1,1,1,1,1,1},
    };

    static constexpr int FLOOR1[10][10] = {
        {1,1,1,1,1,1,1,1,1,1},
        {1,0,0,0,0,0,0,0,0,1},
        {1,0,0,0,0,0,0,0,0,1},
        {1,0,0,0,0,0,0,0,0,1},
        {1,0,0,0,0,0,0,0,0,1},
        {1,3,0,0,0,0,0,0,0,1},
        {1,0,0,0,0,0,0,0,0,1},
        {1,0,0,0,0,0,0,0,0,1},
        {1,0,0,0,0,0,0,0,0,1},
        {1,1,1,1,1,1,1,1,1,1},
    };

    static constexpr float FLOOR_Y = 4.0f; // vertical separation between floors

    GeneratedFloor testFloors[2];
    Entity         playerEntity   = Entity{std::numeric_limits<entity_id_type>::max()};
    Entity         debugCamEntity = Entity{std::numeric_limits<entity_id_type>::max()};

    void buildTestFloors();
    void spawnFloor(int floorIdx);
    void spawnRamp();
    void spawnPlayer(SceneContext& ctx);
    void spawnDebugCamera(SceneContext& ctx);
};
