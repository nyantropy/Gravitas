#pragma once

#include "ECSControllerSystem.hpp"
#include "ECSWorld.hpp"
#include "SceneContext.h"

#include "PlayerComponent.h"
#include "TransformComponent.h"
#include "CameraDescriptionComponent.h"
#include "StairComponent.h"
#include "DungeonTransitionState.h"

#include <string>

// Checks whether the player is standing on a stair tile each frame.
// When a match is found the system writes DungeonTransitionState and issues
// a ChangeScene command to load the target floor scene ("floor_N").
//
// Note: ChangeScene is processed after the current frame completes, so the
// DungeonTransitionState is guaranteed to be set before the next scene's
// onLoad is called.
class StairInteractionSystem : public ECSControllerSystem
{
public:
    void update(ECSWorld& world, SceneContext& ctx) override
    {
        // Skip stair interaction entirely when the debug camera is active.
        bool playerCamActive = false;
        world.forEach<PlayerComponent, CameraDescriptionComponent>(
            [&](Entity, PlayerComponent&, CameraDescriptionComponent& desc)
        {
            if (desc.active) playerCamActive = true;
        });
        if (!playerCamActive) return;

        // Collect player grid position (only when not in transition)
        int playerX = -1;
        int playerZ = -1;

        world.forEach<PlayerComponent>(
            [&](Entity, PlayerComponent& p)
        {
            if (!p.inTransition)
            {
                playerX = p.gridX;
                playerZ = p.gridZ;
            }
        });

        if (playerX < 0) return;

        // Check each stair tile
        world.forEach<StairComponent, TransformComponent>(
            [&](Entity, StairComponent& stair, TransformComponent& tc)
        {
            const int sx = static_cast<int>(tc.position.x);
            const int sz = static_cast<int>(tc.position.z);

            if (playerX != sx || playerZ != sz) return;

            // Record the current floor number (floor - 1 means we came up from below)
            DungeonTransitionState& ts = DungeonTransitionState::instance();
            ts.playerGridPos           = {playerX, playerZ};

            // fromFloor encodes which floor we are leaving
            // DungeonFloorScene::spawnPlayer uses this to place the player at the
            // correct staircase on the destination floor.
            // We derive the current floor from targetFloor:
            //   going Down: targetFloor = current + 1  → fromFloor = targetFloor - 1
            //   going Up:   targetFloor = current - 1  → fromFloor = targetFloor + 1
            if (stair.direction == StairDirection::Down)
                ts.fromFloor = stair.targetFloor - 1;
            else
                ts.fromFloor = stair.targetFloor + 1;

            const std::string sceneName = "floor_" + std::to_string(stair.targetFloor);
            ctx.engineCommands->requestChangeScene(sceneName);
        });
    }
};
