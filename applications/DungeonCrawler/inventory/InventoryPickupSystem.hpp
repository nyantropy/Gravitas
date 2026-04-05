#pragma once

#include <unordered_set>
#include <vector>

#include "ECSControllerSystem.hpp"
#include "CollisionEvent.h"
#include "IGtsPhysicsModule.h"
#include "SceneContext.h"

#include "inventory/CollectibleComponent.h"
#include "inventory/CollectibleType.h"
#include "inventory/InventoryEvents.hpp"
#include "inventory/InventoryComponent.h"
#include "components/PlayerComponent.h"

class InventoryPickupSystem : public ECSControllerSystem
{
public:
    explicit InventoryPickupSystem(IGtsPhysicsModule* physicsModule)
        : physicsModule(physicsModule)
    {
    }

    void update(ECSWorld& world, SceneContext& ctx) override
    {
        if (physicsModule == nullptr)
            return;

        const auto& collisions = physicsModule->getCollisions();
        std::unordered_set<entity_id_type> emittedCollectibles;

        for (const CollisionEvent& collision : collisions)
        {
            const Entity playerEntity = resolvePlayerEntity(world, collision);
            const Entity collectibleEntity = resolveCollectibleEntity(world, collision);
            if (playerEntity == INVALID_ENTITY || collectibleEntity == INVALID_ENTITY)
                continue;

            if (!emittedCollectibles.insert(collectibleEntity.id).second)
                continue;

            auto& collectible = world.getComponent<CollectibleComponent>(collectibleEntity);
            if (collectible.type == CollectibleType::Gold)
            {
                ctx.events.emit(GoldPickedUpEvent{
                    playerEntity,
                    collectibleEntity,
                    static_cast<int>(collectible.goldAmount)
                });
                continue;
            }

            ctx.events.emit(ItemPickedUpEvent{playerEntity, collectibleEntity});
            if (collectible.item.id == "health_potion")
            {
                ctx.events.emit(HealthPotionPickedUpEvent{
                    playerEntity,
                    collectibleEntity
                });
            }
        } 
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

    Entity resolveCollectibleEntity(ECSWorld& world, const CollisionEvent& collision) const
    {
        if (world.hasComponent<CollectibleComponent>(collision.a))
            return collision.a;

        if (world.hasComponent<CollectibleComponent>(collision.b))
            return collision.b;

        return INVALID_ENTITY;
    }
};
