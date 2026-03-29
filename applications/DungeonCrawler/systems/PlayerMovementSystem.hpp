#pragma once

#include "GlmConfig.h"
#include "ECSControllerSystem.hpp"
#include "ECSWorld.hpp"
#include "SceneContext.h"

#include "PlayerComponent.h"
#include "TransformComponent.h"
#include "CameraDescriptionComponent.h"
#include "DungeonInputComponent.h"
#include "DungeonFloorSingleton.h"

// Handles player grid movement and facing — pure game logic, no camera code.
// Reads DungeonInputComponent singleton (written by DungeonInputSystem).
// Input is locked while a movement or turn transition is in progress.
// On valid input, records from/to positions and yaw, then advances
// transitionProgress each frame. PlayerCameraSystem reads the progress
// to interpolate the visual camera position and orientation.
class PlayerMovementSystem : public ECSControllerSystem
{
public:
    void update(ECSWorld& world, SceneContext& ctx) override
    {
        const float dt    = ctx.time->unscaledDeltaTime;
        auto&       input = world.getSingleton<DungeonInputComponent>();
        auto&       floorSingleton = world.getSingleton<DungeonFloorSingleton>();

        world.forEach<PlayerComponent, TransformComponent, CameraDescriptionComponent>(
            [&](Entity, PlayerComponent& player, TransformComponent& tc,
                CameraDescriptionComponent& desc)
        {
            if (!desc.active) return; // debug camera active — skip player input

            // Advance active transition
            if (player.inTransition)
            {
                player.transitionProgress += dt / PlayerComponent::TRANSITION_DURATION;
                if (player.transitionProgress >= 1.0f)
                {
                    player.transitionProgress = 1.0f;
                    player.inTransition       = false;

                    // Snap transform to final position
                    tc.position = player.toPosition;
                }
                return; // input locked during transition
            }

            // --- Accept new input when idle ---

            // Turning (Q / E)
            int newFacing = player.facing;
            if      (input.turnLeft)  newFacing = (player.facing + 3) % 4;
            else if (input.turnRight) newFacing = (player.facing + 1) % 4;

            if (newFacing != player.facing)
            {
                player.fromYaw = facingToYaw(player.facing);
                player.toYaw   = facingToYaw(newFacing);
                player.facing  = newFacing;
                startTransition(player, tc.position, tc.position);
                return;
            }

            // Grid movement (WASD)
            glm::ivec2 forward = facingToForward(player.facing);
            glm::ivec2 right   = facingToRight(player.facing);
            glm::ivec2 move    = {0, 0};

            if      (input.moveForward)  move =  forward;
            else if (input.moveBackward) move = -forward;
            else if (input.strafeLeft)   move = -right;
            else if (input.strafeRight)  move =  right;

            if (move != glm::ivec2{0, 0})
            {
                int newX = player.gridX + move.x;
                int newZ = player.gridZ + move.y;

                if (floorSingleton.floor && floorSingleton.floor->isWalkable(newX, newZ))
                {
                    glm::vec3 newPos = {newX + 0.5f, 0.5f, newZ + 0.5f};
                    player.gridX   = newX;
                    player.gridZ   = newZ;
                    player.fromYaw = facingToYaw(player.facing);
                    player.toYaw   = player.fromYaw; // no yaw change on move
                    startTransition(player, tc.position, newPos);
                }
            }
        });
    }

private:
    static void startTransition(PlayerComponent& player,
                                const glm::vec3& from,
                                const glm::vec3& to)
    {
        player.fromPosition       = from;
        player.toPosition         = to;
        player.transitionProgress = 0.0f;
        player.inTransition       = true;
    }

    // Yaw convention: North=0°, East=90°, South=180°, West=270°
    // forward vector from yaw: (sin(yaw), 0, -cos(yaw))
    static float facingToYaw(int facing)
    {
        switch (facing)
        {
            case 0: return   0.0f; // North
            case 1: return  90.0f; // East
            case 2: return 180.0f; // South
            case 3: return 270.0f; // West
            default: return 0.0f;
        }
    }

    static glm::ivec2 facingToForward(int facing)
    {
        switch (facing)
        {
            case 0: return { 0, -1}; // North = -Z
            case 1: return { 1,  0}; // East  = +X
            case 2: return { 0,  1}; // South = +Z
            case 3: return {-1,  0}; // West  = -X
            default: return {0, 0};
        }
    }

    static glm::ivec2 facingToRight(int facing)
    {
        return facingToForward((facing + 1) % 4);
    }
};
