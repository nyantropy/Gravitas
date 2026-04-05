#pragma once

#include <vector>

#include "ECSSimulationSystem.hpp"
#include "CollisionEvent.h"
#include "IGtsPhysicsModule.h"

#include "InventoryComponent.h"
#include "KeyCollectibleComponent.h"
#include "KeySpawnState.h"
#include "PlayerComponent.h"

class InventoryPickupSystem : public ECSSimulationSystem
{
public:
    explicit InventoryPickupSystem(IGtsPhysicsModule* physicsModule)
        : physicsModule(physicsModule)
    {
    }

    void update(ECSWorld& world, float /*dt*/) override
    {
        if (physicsModule == nullptr)
            return;

        const auto& collisions = physicsModule->getCollisions();
        std::vector<Entity> pickedKeys;

        for (const CollisionEvent& collision : collisions)
        {
            const Entity playerEntity = resolvePlayerEntity(world, collision);
            const Entity keyEntity = resolveKeyEntity(world, collision);
            if (playerEntity == INVALID_ENTITY || keyEntity == INVALID_ENTITY)
                continue;

            auto& inventory = world.getComponent<InventoryComponent>(playerEntity);
            const auto& key = world.getComponent<KeyCollectibleComponent>(keyEntity);
            if (!inventory.addItem(key.item))
                continue;

            if (world.hasAny<KeySpawnState>())
                world.getSingleton<KeySpawnState>().collected = true;

            pickedKeys.push_back(keyEntity);
            break;
        }

        for (Entity keyEntity : pickedKeys)
            world.destroyEntity(keyEntity);
    }

private:
    IGtsPhysicsModule* physicsModule = nullptr;

    Entity resolvePlayerEntity(ECSWorld& world, const CollisionEvent& collision) const
    {
        if (world.hasComponent<PlayerComponent>(collision.a)
            && world.hasComponent<InventoryComponent>(collision.a))
            return collision.a;

        if (world.hasComponent<PlayerComponent>(collision.b)
            && world.hasComponent<InventoryComponent>(collision.b))
            return collision.b;

        return INVALID_ENTITY;
    }

    Entity resolveKeyEntity(ECSWorld& world, const CollisionEvent& collision) const
    {
        if (world.hasComponent<KeyCollectibleComponent>(collision.a))
            return collision.a;

        if (world.hasComponent<KeyCollectibleComponent>(collision.b))
            return collision.b;

        return INVALID_ENTITY;
    }
};
