#pragma once

#include <vector>
#include <algorithm>

#include "GlmConfig.h"
#include "ECSControllerSystem.hpp"
#include "ECSWorld.hpp"
#include "SceneContext.h"

#include "components/PlayerComponent.h"
#include "components/EnemyComponent.h"
#include "components/KeyItemComponent.h"
#include "components/ExitComponent.h"
#include "components/DungeonInputComponent.h"
#include "components/DungeonGameStateComponent.h"

// Controller system — player attacks and world interaction.
//   Space : strike the cell directly ahead; deal 1 damage to any living enemy there.
//   Walk onto key cell : pick up the key (entity destroyed, hasKey = true).
//   Walk onto exit with key : win condition.
class CombatSystem : public ECSControllerSystem
{
public:
    void update(ECSWorld& world, SceneContext& ctx) override
    {
        auto& state = world.getSingleton<DungeonGameStateComponent>();
        if (state.result != DungeonGameResult::Playing) return;

        const float       dt    = ctx.time->unscaledDeltaTime;
        const auto& input = world.getSingleton<DungeonInputComponent>();

        state.attackCooldown = std::max(0.0f, state.attackCooldown - dt);
        state.messageTimer   = std::max(0.0f, state.messageTimer   - dt);
        if (state.messageTimer <= 0.0f) state.message = "";

        // Capture player state.
        int px = -1, pz = -1, pFacing = 0;
        world.forEach<PlayerComponent>([&](Entity, PlayerComponent& p)
        {
            px = p.gridX; pz = p.gridZ; pFacing = p.facing;
        });
        if (px < 0) return;

        const glm::ivec2 fwd  = facingToForward(pFacing);
        const int        fx   = px + fwd.x;
        const int        fz   = pz + fwd.y;

        // --- Attack (Space) ---
        if (input.attackPressed && state.attackCooldown <= 0.0f)
        {
            state.attackCooldown = DungeonGameStateComponent::ATTACK_COOLDOWN;
            bool hit = false;

            world.forEach<EnemyComponent>([&](Entity, EnemyComponent& enemy)
            {
                if (!enemy.alive || enemy.gridX != fx || enemy.gridZ != fz) return;

                enemy.hp--;
                hit = true;

                if (enemy.hp <= 0)
                {
                    enemy.alive        = false;
                    state.message      = "ENEMY SLAIN";
                    state.messageTimer = 2.0f;
                }
                else
                {
                    state.message      = "HIT";
                    state.messageTimer = 0.5f;
                }
            });

            if (!hit)
            {
                state.message      = "SWING";
                state.messageTimer = 0.3f;
            }
        }

        // --- Clean up dead enemies (after iteration, to avoid invalidating forEach) ---
        {
            std::vector<Entity> dead;
            world.forEach<EnemyComponent>([&](Entity e, EnemyComponent& enemy)
            {
                if (!enemy.alive) dead.push_back(e);
            });
            for (Entity e : dead) world.destroyEntity(e);
        }

        // --- Key pickup ---
        if (!state.hasKey)
        {
            Entity keyEntity{};
            bool   found = false;
            world.forEach<KeyItemComponent>([&](Entity e, KeyItemComponent& key)
            {
                if (key.gridX == px && key.gridZ == pz)
                {
                    keyEntity = e;
                    found     = true;
                }
            });
            if (found)
            {
                state.hasKey       = true;
                state.message      = "KEY FOUND  EXIT UNLOCKED";
                state.messageTimer = 3.0f;
                world.destroyEntity(keyEntity);
            }
        }

        // --- Exit check ---
        world.forEach<ExitComponent>([&](Entity, ExitComponent& exit)
        {
            if (exit.gridX == px && exit.gridZ == pz && state.hasKey)
            {
                state.result       = DungeonGameResult::Won;
                state.message      = "YOU ESCAPED";
                state.messageTimer = 999.0f;
            }
        });
    }

private:
    static glm::ivec2 facingToForward(int facing)
    {
        switch (facing)
        {
            case 0: return { 0, -1}; // North
            case 1: return { 1,  0}; // East
            case 2: return { 0,  1}; // South
            case 3: return {-1,  0}; // West
            default: return {0, 0};
        }
    }
};
