#pragma once

#include <algorithm>

#include "GlmConfig.h"
#include "ECSControllerSystem.hpp"
#include "ECSWorld.hpp"
#include "SceneContext.h"

#include "PlayerComponent.h"
#include "TransformComponent.h"
#include "DungeonInputComponent.h"
#include "DungeonMap.h"

// Handles player grid movement and facing — pure game logic, no camera code.
// Reads DungeonInputComponent singleton (written by DungeonInputSystem).
// Ticks cooldown timers so holding a key moves one cell at a time.
// Q/E rotate facing 90 degrees; WASD move relative to current facing.
// Updates TransformComponent.position (eye-height world position).
class PlayerMovementSystem : public ECSControllerSystem
{
public:
    void update(ECSWorld& world, SceneContext& ctx) override
    {
        const float dt    = ctx.time->unscaledDeltaTime;
        auto&       input = world.getSingleton<DungeonInputComponent>();

        world.forEach<PlayerComponent, TransformComponent>(
            [&](Entity, PlayerComponent& player, TransformComponent& tc)
        {
            // Tick cooldowns.
            player.moveCooldown = std::max(0.0f, player.moveCooldown - dt);
            player.turnCooldown = std::max(0.0f, player.turnCooldown - dt);

            // --- Turning (Q / E) ---
            if (player.turnCooldown <= 0.0f)
            {
                if (input.turnLeft)
                {
                    player.facing       = (player.facing + 3) % 4; // -1 mod 4
                    player.turnCooldown = PlayerComponent::TURN_COOLDOWN_TIME;
                }
                else if (input.turnRight)
                {
                    player.facing       = (player.facing + 1) % 4;
                    player.turnCooldown = PlayerComponent::TURN_COOLDOWN_TIME;
                }
            }

            // --- Grid movement (WASD) ---
            if (player.moveCooldown <= 0.0f)
            {
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

                    if (isWalkable(newX, newZ))
                    {
                        player.gridX        = newX;
                        player.gridZ        = newZ;
                        player.moveCooldown = PlayerComponent::MOVE_COOLDOWN_TIME;
                    }
                }
            }

            // Cell size = 1.0 world unit; eye height = 0.5 units.
            tc.position = glm::vec3(
                player.gridX + 0.5f,
                0.5f,
                player.gridZ + 0.5f);
        });
    }

private:
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
