#include "player/PlayerMarkerSystem.h"

#include <limits>

#include "BoundsComponent.h"
#include "GraphicsConstants.h"
#include "MaterialComponent.h"
#include "ProceduralMeshComponent.h"
#include "TransformComponent.h"
#include "components/PlayerComponent.h"
#include "ECSWorld.hpp"

Entity PlayerMarkerSystem::spawnMarker(ECSWorld& world) const
{
    Entity markerEntity = world.createEntity();

    ProceduralMeshComponent mesh;
    mesh.width  = 0.6f;
    mesh.height = 0.6f;
    world.addComponent(markerEntity, mesh);

    MaterialComponent material;
    material.texturePath = GraphicsConstants::ENGINE_RESOURCES + "/pictures/furina.jpg";
    material.doubleSided = true;
    world.addComponent(markerEntity, material);

    TransformComponent transform;
    transform.rotation.x = -glm::half_pi<float>();
    transform.position   = {0.0f, 0.1f, 0.0f};
    world.addComponent(markerEntity, transform);

    BoundsComponent bounds;
    bounds.min = {-0.3f, -0.3f, -0.02f};
    bounds.max = { 0.3f,  0.3f,  0.02f};
    world.addComponent(markerEntity, bounds);

    return markerEntity;
}

void PlayerMarkerSystem::syncMarker(ECSWorld& world, Entity playerEntity, Entity markerEntity) const
{
    constexpr Entity invalidEntity{std::numeric_limits<entity_id_type>::max()};
    if (markerEntity == invalidEntity || playerEntity == invalidEntity)
        return;

    const auto& player = world.getComponent<PlayerComponent>(playerEntity);
    auto& markerTransform = world.getComponent<TransformComponent>(markerEntity);
    markerTransform.position = {player.gridX + 0.5f, 0.1f, player.gridZ + 0.5f};
}
