#pragma once

#include <vector>

#include "ECSSimulationSystem.hpp"
#include "CollisionEvent.h"
#include "IGtsPhysicsModule.h"

#include "inventory/CollectibleComponent.h"
#include "inventory/CollectibleRunState.h"
#include "inventory/CollectibleType.h"
#include "inventory/GoldComponent.h"
#include "inventory/InventoryComponent.h"
#include "components/PlayerComponent.h"

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
        std::vector<Entity> pickedCollectibles;

        for (const CollisionEvent& collision : collisions)
        {
            const Entity playerEntity = resolvePlayerEntity(world, collision);
            const Entity collectibleEntity = resolveCollectibleEntity(world, collision);
            if (playerEntity == INVALID_ENTITY || collectibleEntity == INVALID_ENTITY)
                continue;

            auto& collectible = world.getComponent<CollectibleComponent>(collectibleEntity);
            bool pickedUp = false;

            if (collectible.type == CollectibleType::Gold)
            {
                auto& gold = world.getComponent<GoldComponent>(playerEntity);
                gold.amount += collectible.goldAmount;
                pickedUp = true;
            }
            else
            {
                auto& inventory = world.getComponent<InventoryComponent>(playerEntity);
                pickedUp = inventory.addItem(collectible.item);
            }

            if (!pickedUp)
                continue;

            if (world.hasAny<CollectibleRunState>())
            {
                auto& runState = world.getSingleton<CollectibleRunState>();
                for (auto& spawn : runState.collectibles)
                {
                    if (spawn.floorIndex == collectible.floorIndex
                        && spawn.gridPosition == collectible.gridPosition
                        && spawn.type == collectible.type)
                    {
                        spawn.collected = true;
                        break;
                    }
                }
            }

            pickedCollectibles.push_back(collectibleEntity);
        }

        for (Entity collectibleEntity : pickedCollectibles)
            world.destroyEntity(collectibleEntity);
    }

private:
    IGtsPhysicsModule* physicsModule = nullptr;

    Entity resolvePlayerEntity(ECSWorld& world, const CollisionEvent& collision) const
    {
        if (world.hasComponent<PlayerComponent>(collision.a)
            && world.hasComponent<InventoryComponent>(collision.a)
            && world.hasComponent<GoldComponent>(collision.a))
            return collision.a;

        if (world.hasComponent<PlayerComponent>(collision.b)
            && world.hasComponent<InventoryComponent>(collision.b)
            && world.hasComponent<GoldComponent>(collision.b))
            return collision.b;

        return INVALID_ENTITY;
    }

    Entity resolveCollectibleEntity(ECSWorld& world, const CollisionEvent& collision) const
    {
        if (world.hasComponent<CollectibleComponent>(collision.a))
            return collision.a;

        if (world.hasComponent<CollectibleComponent>(collision.b))
            return collision.b;

        return INVALID_ENTITY;
    }
};
