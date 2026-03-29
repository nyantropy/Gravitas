#pragma once

#include "GlmConfig.h"
#include "ECSControllerSystem.hpp"
#include "ECSWorld.hpp"
#include "SceneContext.h"

#include "FloorEnemyComponent.h"
#include "TransformComponent.h"

// Advances FloorEnemyComponent patrol movement each frame.
// Enemies ping-pong along their stored patrol path with smooth lerp transitions.
// When a transition completes the enemy waits PATROL_WAIT_TIME seconds before
// advancing to the next waypoint.
class EnemyMovementSystem : public ECSControllerSystem
{
public:
    void update(ECSWorld& world, SceneContext& ctx) override
    {
        const float dt = ctx.time->unscaledDeltaTime;

        world.forEach<FloorEnemyComponent, TransformComponent>(
            [&](Entity, FloorEnemyComponent& enemy, TransformComponent& tc)
        {
            if (!enemy.alive) return;

            // Advance active transition
            if (enemy.inTransition)
            {
                enemy.transitionProgress += dt / FloorEnemyComponent::TRANSITION_DURATION;

                if (enemy.transitionProgress >= 1.0f)
                {
                    enemy.transitionProgress = 1.0f;
                    enemy.inTransition       = false;
                    tc.position              = enemy.toPosition;
                    enemy.waitTimer          = FloorEnemyComponent::PATROL_WAIT_TIME;
                }

                const float t    = enemy.transitionProgress;
                const float ease = t * t * (3.0f - 2.0f * t); // smoothstep
                tc.position = glm::mix(enemy.fromPosition, enemy.toPosition, ease);
                return;
            }

            // Wait at current waypoint
            if (enemy.waitTimer > 0.0f)
            {
                enemy.waitTimer -= dt;
                return;
            }

            // No path — nothing to do
            if (enemy.patrolPath.empty()) return;

            // Advance patrol index (ping-pong)
            int nextIdx = enemy.patrolIndex + (enemy.patrolForward ? 1 : -1);

            if (nextIdx >= static_cast<int>(enemy.patrolPath.size()))
            {
                enemy.patrolForward = false;
                nextIdx = static_cast<int>(enemy.patrolPath.size()) - 2;
            }
            else if (nextIdx < 0)
            {
                enemy.patrolForward = true;
                nextIdx = 1;
            }

            if (nextIdx < 0 || nextIdx >= static_cast<int>(enemy.patrolPath.size()))
                return;

            enemy.patrolIndex = nextIdx;

            glm::ivec2 target   = enemy.patrolPath[nextIdx];
            enemy.fromPosition  = tc.position;
            enemy.toPosition    = glm::vec3(target.x + 0.5f, 0.5f, target.y + 0.5f);
            enemy.gridX         = target.x;
            enemy.gridZ         = target.y;
            enemy.inTransition  = true;
            enemy.transitionProgress = 0.0f;
        });
    }
};
