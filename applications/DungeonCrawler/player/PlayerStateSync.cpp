#include "player/PlayerStateSync.h"

#include <limits>

#include "CameraDescriptionComponent.h"
#include "TransformComponent.h"
#include "components/PlayerComponent.h"
#include "ECSWorld.hpp"
#include "inventory/GoldComponent.h"
#include "inventory/InventoryComponent.h"

namespace
{
    glm::vec3 gridToWorld(const glm::ivec2& gridPos)
    {
        return {gridPos.x + 0.5f, 0.5f, gridPos.y + 0.5f};
    }
}

void PlayerStateSync::movePlayerToTile(ECSWorld& world,
                                       Entity playerEntity,
                                       const glm::ivec2& gridPos) const
{
    auto& player    = world.getComponent<PlayerComponent>(playerEntity);
    auto& transform = world.getComponent<TransformComponent>(playerEntity);
    auto& camera    = world.getComponent<CameraDescriptionComponent>(playerEntity);

    const glm::vec3 worldPos = gridToWorld(gridPos);

    player.gridX = gridPos.x;
    player.gridZ = gridPos.y;
    player.fromPosition = worldPos;
    player.toPosition   = worldPos;
    player.inTransition = false;
    player.transitionProgress = 1.0f;
    transform.position = worldPos;

    const float yawRad = glm::radians(player.toYaw);
    camera.target = worldPos + glm::vec3(glm::sin(yawRad), 0.0f, -glm::cos(yawRad));
}

void PlayerStateSync::syncPersistentState(ECSWorld& world,
                                          Entity playerEntity,
                                          std::vector<Item>& inventoryItems,
                                          uint32_t& goldAmount) const
{
    constexpr Entity invalidEntity{std::numeric_limits<entity_id_type>::max()};
    if (playerEntity == invalidEntity)
        return;

    if (world.hasComponent<InventoryComponent>(playerEntity))
        inventoryItems = world.getComponent<InventoryComponent>(playerEntity).items;

    if (world.hasComponent<GoldComponent>(playerEntity))
        goldAmount = world.getComponent<GoldComponent>(playerEntity).amount;
}
