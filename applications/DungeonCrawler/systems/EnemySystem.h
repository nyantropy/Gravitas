#pragma once

#include <algorithm>

#include "ECSControllerSystem.hpp"
#include "ECSWorld.hpp"
#include "SceneContext.h"

#include "components/EnemyComponent.h"
#include "TransformComponent.h"
#include "components/PlayerComponent.h"
#include "components/DungeonGameStateComponent.h"
#include "dungeon/DungeonMap.h"

// Controller system — enemy patrol AI.
// Each enemy walks back and forth between its two patrol waypoints.
// When an enemy tries to step onto the player's cell it deals 1 damage
// instead (bump attack) and the player receives a brief invincibility window.
class EnemySystem : public ECSControllerSystem
{
public:
    void update(ECSWorld& world, SceneContext& ctx) override
    {
        auto& state = world.getSingleton<DungeonGameStateComponent>();
        if (state.result != DungeonGameResult::Playing) return;

        const float dt = ctx.time->unscaledDeltaTime;

        // Capture player grid position for bump-attack detection.
        int px = -1, pz = -1;
        world.forEach<PlayerComponent>([&](Entity, PlayerComponent& p)
        {
            px = p.gridX;
            pz = p.gridZ;
        });

        state.hurtCooldown = std::max(0.0f, state.hurtCooldown - dt);

        world.forEach<EnemyComponent, TransformComponent>(
            [&](Entity e, EnemyComponent& enemy, TransformComponent& tc)
        {
            if (!enemy.alive) return;

            // Always sync visual position.
            tc.position = {enemy.gridX + 0.5f, 0.5f, enemy.gridZ + 0.5f};

            enemy.moveCooldown = std::max(0.0f, enemy.moveCooldown - dt);
            if (enemy.moveCooldown > 0.0f) return;

            // Determine current patrol target.
            int tX = (enemy.patrolDir > 0) ? enemy.patrolX1 : enemy.patrolX0;
            int tZ = (enemy.patrolDir > 0) ? enemy.patrolZ1 : enemy.patrolZ0;

            // Flip direction when endpoint is reached.
            if (enemy.gridX == tX && enemy.gridZ == tZ)
            {
                enemy.patrolDir = -enemy.patrolDir;
                tX = (enemy.patrolDir > 0) ? enemy.patrolX1 : enemy.patrolX0;
                tZ = (enemy.patrolDir > 0) ? enemy.patrolZ1 : enemy.patrolZ0;
            }

            // Single step toward target (one axis at a time).
            int dx = 0, dz = 0;
            if      (tX > enemy.gridX) dx =  1;
            else if (tX < enemy.gridX) dx = -1;
            else if (tZ > enemy.gridZ) dz =  1;
            else if (tZ < enemy.gridZ) dz = -1;

            const int newX = enemy.gridX + dx;
            const int newZ = enemy.gridZ + dz;

            if (newX == px && newZ == pz)
            {
                // Bump attack — do not occupy player's cell.
                if (state.hurtCooldown <= 0.0f)
                {
                    state.playerHp     = std::max(0, state.playerHp - 1);
                    state.hurtCooldown = DungeonGameStateComponent::HURT_COOLDOWN;

                    if (state.playerHp <= 0)
                    {
                        state.result       = DungeonGameResult::Lost;
                        state.message      = "YOU DIED";
                        state.messageTimer = 999.0f;
                    }
                    else
                    {
                        state.message      = "OUCH";
                        state.messageTimer = 1.0f;
                    }
                }
                enemy.moveCooldown = EnemyComponent::MOVE_COOLDOWN_TIME;
                return;
            }

            if (isWalkable(newX, newZ))
            {
                enemy.gridX = newX;
                enemy.gridZ = newZ;
            }
            else
            {
                // Wall or map boundary — flip and try next tick.
                enemy.patrolDir = -enemy.patrolDir;
            }

            enemy.moveCooldown = EnemyComponent::MOVE_COOLDOWN_TIME;
        });
    }
};
