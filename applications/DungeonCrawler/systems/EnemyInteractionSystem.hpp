#pragma once

#include "ECSSimulationSystem.hpp"

#include "components/BattleEncounterStateComponent.h"
#include "CollisionEvent.h"
#include "components/EnemyComponent.h"
#include "IGtsPhysicsModule.h"
#include "components/PlayerComponent.h"
#include "components/PlayerTagComponent.h"

class EnemyInteractionSystem : public ECSSimulationSystem
{
public:
    explicit EnemyInteractionSystem(IGtsPhysicsModule* physicsModule)
        : physicsModule(physicsModule)
    {
    }

    void update(ECSWorld& world, float /*dt*/) override
    {
        if (physicsModule == nullptr || !world.hasAny<BattleEncounterStateComponent>())
            return;

        auto& encounter = world.getSingleton<BattleEncounterStateComponent>();
        encounter.playerCollisionCount = 0;

        const auto& collisions = physicsModule->getCollisions();
        for (const CollisionEvent& collision : collisions)
        {
            const Entity playerEntity = resolvePlayerEntity(world, collision);
            const Entity enemyEntity = resolveEnemyEntity(world, collision);
            if (playerEntity == INVALID_ENTITY || enemyEntity == INVALID_ENTITY)
                continue;

            ++encounter.playerCollisionCount;

            if (encounter.battleRequested)
                continue;

            const auto& player = world.getComponent<PlayerComponent>(playerEntity);
            const auto& enemy = world.getComponent<EnemyComponent>(enemyEntity);
            if (!enemy.alive)
                continue;

            encounter.battleRequested = true;
            encounter.transitionIssued = false;
            encounter.floorIndex = enemy.floorIndex;
            encounter.playerTile = {player.gridX, player.gridZ};
            encounter.enemyTile = {enemy.gridX, enemy.gridZ};
            encounter.enemySpawnTile = {enemy.spawnGridX, enemy.spawnGridZ};
        }
    }

private:
    IGtsPhysicsModule* physicsModule = nullptr;

    Entity resolvePlayerEntity(ECSWorld& world, const CollisionEvent& collision) const
    {
        if (world.hasComponent<PlayerTagComponent>(collision.a)
            && world.hasComponent<PlayerComponent>(collision.a))
            return collision.a;

        if (world.hasComponent<PlayerTagComponent>(collision.b)
            && world.hasComponent<PlayerComponent>(collision.b))
            return collision.b;

        return INVALID_ENTITY;
    }

    Entity resolveEnemyEntity(ECSWorld& world, const CollisionEvent& collision) const
    {
        if (world.hasComponent<EnemyComponent>(collision.a))
            return collision.a;

        if (world.hasComponent<EnemyComponent>(collision.b))
            return collision.b;

        return INVALID_ENTITY;
    }
};
