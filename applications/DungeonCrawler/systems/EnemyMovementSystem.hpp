#pragma once

#include <array>

#include "GlmConfig.h"
#include "ECSSimulationSystem.hpp"
#include "ECSWorld.hpp"

#include "components/DungeonFloorSingleton.h"
#include "components/EnemyComponent.h"
#include "components/EnemyMovementStateComponent.h"
#include "TransformComponent.h"

// Random-walk movement for grid-bound enemies with smooth interpolation.
class EnemyMovementSystem : public ECSSimulationSystem
{
public:
    void update(ECSWorld& world, float dt) override
    {
        if (!world.hasAny<DungeonFloorSingleton>()) return;

        auto& floorSingleton = world.getSingleton<DungeonFloorSingleton>();

        world.forEach<EnemyComponent, EnemyMovementStateComponent, TransformComponent>(
            [&](Entity e, EnemyComponent& enemy, EnemyMovementStateComponent& movement, TransformComponent& tc)
        {
            if (!enemy.alive) return;
            if (enemy.floorIndex < 0 || enemy.floorIndex >= 4) return;

            const GeneratedFloor* floor = floorSingleton.allFloors[enemy.floorIndex];
            if (!floor) return;

            const glm::vec3 floorOffset = floorSingleton.floorWorldOffset[enemy.floorIndex];

            if (movement.moving)
            {
                movement.progress += dt * enemy.moveSpeed;
                const float clampedProgress = glm::clamp(movement.progress, 0.0f, 1.0f);
                tc.position = glm::mix(movement.startPosition, movement.targetPosition, clampedProgress);

                if (movement.progress >= 1.0f)
                {
                    tc.position       = movement.targetPosition;
                    enemy.gridX       = movement.targetTile.x;
                    enemy.gridZ       = movement.targetTile.y;
                    movement.progress = 1.0f;
                    movement.moving   = false;
                }
                return;
            }

            if (enemy.moveCooldown > 0.0f)
            {
                enemy.moveCooldown = std::max(0.0f, enemy.moveCooldown - dt);
                return;
            }

            const glm::ivec2 nextTile = chooseNextTile(*floor, enemy);
            if (nextTile.x < 0 || nextTile.y < 0)
                return;

            movement.startPosition  = tc.position;
            movement.targetTile     = nextTile;
            movement.targetPosition = {
                floorOffset.x + nextTile.x + 0.5f,
                tc.position.y,
                floorOffset.z + nextTile.y + 0.5f
            };
            movement.progress = 0.0f;
            movement.moving   = true;
        });
    }

private:
    static uint32_t nextRandom(uint32_t& state)
    {
        if (state == 0u) state = 1u;
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return state;
    }

    static glm::ivec2 chooseNextTile(const GeneratedFloor& floor, EnemyComponent& enemy)
    {
        static constexpr std::array<glm::ivec2, 4> directions = {{
            {0, -1},
            {1, 0},
            {0, 1},
            {-1, 0},
        }};

        const int startDirection = static_cast<int>(nextRandom(enemy.rngState) % directions.size());
        for (int i = 0; i < static_cast<int>(directions.size()); ++i)
        {
            const glm::ivec2 dir = directions[(startDirection + i) % directions.size()];
            const glm::ivec2 candidate = {enemy.gridX + dir.x, enemy.gridZ + dir.y};
            if (!floor.isWalkable(candidate.x, candidate.y)) continue;
            if (floor.get(candidate.x, candidate.y) == TileType::Wall) continue;
            return candidate;
        }

        return {-1, -1};
    }
};
