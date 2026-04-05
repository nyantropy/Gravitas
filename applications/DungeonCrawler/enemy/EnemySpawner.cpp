#include "enemy/EnemySpawner.h"

#include <algorithm>
#include <string>

#include "BoundsComponent.h"
#include "GraphicsConstants.h"
#include "MaterialComponent.h"
#include "PhysicsBodyComponent.h"
#include "SphereColliderComponent.h"
#include "StaticMeshComponent.h"
#include "TransformComponent.h"
#include "components/EnemyComponent.h"
#include "components/EnemyMovementStateComponent.h"
#include "components/EnemyTagComponent.h"
#include "ECSWorld.hpp"

void EnemySpawner::spawnEnemies(ECSWorld& world,
                                std::vector<Entity>& floorEntities,
                                const GeneratedFloor& floor) const
{
    const std::string cubeMesh = GraphicsConstants::ENGINE_RESOURCES + "/models/cube.obj";
    const std::string blueTexture = GraphicsConstants::ENGINE_RESOURCES + "/textures/blue_texture.png";

    for (size_t spawnIndex = 0; spawnIndex < floor.enemySpawnPositions.size(); ++spawnIndex)
    {
        const glm::vec3& spawnPosition = floor.enemySpawnPositions[spawnIndex];
        Entity entity = world.createEntity();
        floorEntities.push_back(entity);

        TransformComponent transform;
        transform.position = spawnPosition;
        transform.scale    = {0.5f, 0.5f, 0.5f};

        EnemyComponent enemy;
        enemy.gridX      = static_cast<int>(spawnPosition.x);
        enemy.gridZ      = static_cast<int>(spawnPosition.z);
        enemy.spawnGridX = enemy.gridX;
        enemy.spawnGridZ = enemy.gridZ;
        enemy.floorIndex = floor.floorNumber;
        enemy.moveSpeed  = 2.0f + static_cast<float>((floor.floorNumber * 13 + static_cast<int>(spawnIndex) * 17) % 11) * 0.1f;
        enemy.rngState   = static_cast<uint32_t>((floor.floorNumber + 1) * 73856093
                       ^ (enemy.gridX + 1) * 19349663
                       ^ (enemy.gridZ + 1) * 83492791
                       ^ static_cast<int>(spawnIndex + 1) * 2654435761u);

        EnemyMovementStateComponent movement;
        movement.startPosition  = spawnPosition;
        movement.targetPosition = spawnPosition;
        movement.targetTile     = {enemy.gridX, enemy.gridZ};

        PhysicsBodyComponent body;
        SphereColliderComponent collider;
        collider.radius = 0.35f;

        StaticMeshComponent mesh;
        mesh.meshPath = cubeMesh;

        MaterialComponent material;
        material.texturePath = blueTexture;
        material.tint        = {0.25f, 0.45f, 1.0f, 1.0f};

        BoundsComponent bounds;
        bounds.min = {-0.25f, -0.25f, -0.25f};
        bounds.max = { 0.25f,  0.25f,  0.25f};

        world.addComponent(entity, enemy);
        world.addComponent(entity, movement);
        world.addComponent(entity, body);
        world.addComponent(entity, collider);
        world.addComponent(entity, EnemyTagComponent{});
        world.addComponent(entity, transform);
        world.addComponent(entity, mesh);
        world.addComponent(entity, material);
        world.addComponent(entity, bounds);
    }
}

void EnemySpawner::removeDefeatedEnemy(DungeonManager& dungeon,
                                       int floorIndex,
                                       const glm::ivec2& spawnTile) const
{
    GeneratedFloor* floor = dungeon.getFloor(floorIndex);
    if (floor == nullptr)
        return;

    auto& enemySpawns = floor->enemySpawnPositions;
    enemySpawns.erase(
        std::remove_if(
            enemySpawns.begin(),
            enemySpawns.end(),
            [&](const glm::vec3& spawnPosition)
            {
                return static_cast<int>(spawnPosition.x) == spawnTile.x
                    && static_cast<int>(spawnPosition.z) == spawnTile.y;
            }),
        enemySpawns.end());
}
