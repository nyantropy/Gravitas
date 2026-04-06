#include "player/PlayerSpawner.h"

#include "CameraDescriptionComponent.h"
#include "PhysicsBodyComponent.h"
#include "SphereColliderComponent.h"
#include "TransformComponent.h"
#include "components/PlayerComponent.h"
#include "components/PlayerTagComponent.h"
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

Entity PlayerSpawner::spawnPlayer(ECSWorld& world,
                                  SceneContext& ctx,
                                  const glm::ivec2& startPos,
                                  const std::vector<Item>& inventoryItems,
                                  uint32_t goldAmount) const
{
    Entity playerEntity = world.createEntity();

    PlayerComponent player;
    player.gridX = startPos.x;
    player.gridZ = startPos.y;
    player.facing = 1;
    player.fromPosition = gridToWorld(startPos);
    player.toPosition   = gridToWorld(startPos);
    player.fromYaw = 90.0f;
    player.toYaw   = 90.0f;
    world.addComponent(playerEntity, player);
    world.addComponent(playerEntity, PlayerTagComponent{});
    world.addComponent(playerEntity, InventoryComponent{
        inventoryItems,
        8
    });
    world.addComponent(playerEntity, GoldComponent{goldAmount});

    TransformComponent transform;
    transform.position = gridToWorld(startPos);
    world.addComponent(playerEntity, transform);

    PhysicsBodyComponent body;
    SphereColliderComponent collider;
    collider.radius = 0.4f;
    world.addComponent(playerEntity, body);
    world.addComponent(playerEntity, collider);

    CameraDescriptionComponent camera;
    camera.active      = true;
    camera.fov         = glm::radians(70.0f);
    camera.aspectRatio = ctx.windowAspectRatio;
    camera.nearClip    = 0.05f;
    camera.farClip     = 100.0f;
    camera.target      = transform.position + glm::vec3(1.0f, 0.0f, 0.0f);
    world.addComponent(playerEntity, camera);

    return playerEntity;
}
