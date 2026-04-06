#pragma once

#include <algorithm>
#include <cstdint>

#include "ECSControllerSystem.hpp"
#include "SceneContext.h"

#include "inventory/CollectibleComponent.h"
#include "inventory/CollectibleRunState.h"
#include "inventory/CollectibleType.h"
#include "inventory/GoldComponent.h"
#include "inventory/InventoryComponent.h"
#include "inventory/InventoryEvents.hpp"

class InventoryEventSystem : public ECSControllerSystem
{
public:
    void update(ECSWorld& world, SceneContext& ctx) override
    {
        if (subscriptionsInitialized)
            return;

        initializeSubscriptions(world, ctx);
        subscriptionsInitialized = true;
    }

private:
    bool subscriptionsInitialized = false;

    void initializeSubscriptions(ECSWorld& world, SceneContext& ctx)
    {
        ctx.events.subscribe<ItemPickedUpEvent>(
            [&world](const ItemPickedUpEvent& event)
            {
                if (!world.hasComponent<CollectibleComponent>(event.item))
                    return;

                auto& collectible = world.getComponent<CollectibleComponent>(event.item);
                if (collectible.type != CollectibleType::Gold)
                {
                    if (!world.hasComponent<InventoryComponent>(event.player))
                        return;

                    auto& inventory = world.getComponent<InventoryComponent>(event.player);
                    if (!inventory.addItem(collectible.item))
                        return;
                }

                markCollected(world, collectible);
                world.destroyEntity(event.item);
            });

        ctx.events.subscribe<GoldPickedUpEvent>(
            [&world](const GoldPickedUpEvent& event)
            {
                if (!world.hasComponent<GoldComponent>(event.player))
                    return;

                auto& gold = world.getComponent<GoldComponent>(event.player);
                gold.amount += static_cast<uint32_t>(std::max(event.amount, 0));
            });
    }

    static void markCollected(ECSWorld& world, const CollectibleComponent& collectible)
    {
        if (!world.hasAny<CollectibleRunState>())
            return;

        auto& runState = world.getSingleton<CollectibleRunState>();
        for (auto& spawn : runState.collectibles)
        {
            if (spawn.floorIndex == collectible.floorIndex
                && spawn.gridPosition == collectible.gridPosition
                && spawn.type == collectible.type)
            {
                spawn.collected = true;
                return;
            }
        }
    }
};
