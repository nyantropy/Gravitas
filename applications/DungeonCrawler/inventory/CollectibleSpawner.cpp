#include "inventory/CollectibleSpawner.h"

#include <algorithm>
#include <random>
#include <string>

#include "BoundsComponent.h"
#include "GraphicsConstants.h"
#include "MaterialComponent.h"
#include "PhysicsBodyComponent.h"
#include "SphereColliderComponent.h"
#include "StaticMeshComponent.h"
#include "TransformComponent.h"
#include "ECSWorld.hpp"
#include "inventory/CollectibleComponent.h"
#include "inventory/CollectibleSpawnState.h"
#include "inventory/CollectibleType.h"

namespace
{
    Item makeGoldenKeyItem()
    {
        return {
            "key",
            "Golden Key",
            GraphicsConstants::ENGINE_RESOURCES + "/textures/yellow_texture.png"
        };
    }

    Item makeHealthPotionItem()
    {
        return {
            "health_potion",
            "Health Potion",
            GraphicsConstants::ENGINE_RESOURCES + "/textures/red_texture.png"
        };
    }
}

void CollectibleSpawner::spawnCollectibles(ECSWorld& world,
                                           std::vector<Entity>& floorEntities,
                                           const GeneratedFloor& floor,
                                           const CollectibleRunState& runState) const
{
    const std::string cubeMesh = GraphicsConstants::ENGINE_RESOURCES + "/models/cube.obj";

    for (const auto& spawn : runState.collectibles)
    {
        if (spawn.collected || spawn.floorIndex != floor.floorNumber)
            continue;

        Entity entity = world.createEntity();
        floorEntities.push_back(entity);

        TransformComponent transform;
        transform.position = {spawn.gridPosition.x + 0.5f, 0.7f, spawn.gridPosition.y + 0.5f};

        MaterialComponent material;
        CollectibleComponent collectible;
        collectible.type = spawn.type;
        collectible.item = spawn.item;
        collectible.goldAmount = spawn.goldAmount;
        collectible.floorIndex = spawn.floorIndex;
        collectible.gridPosition = spawn.gridPosition;

        if (spawn.type == CollectibleType::Gold)
        {
            transform.scale = {0.28f, 0.28f, 0.28f};
            material.texturePath = GraphicsConstants::ENGINE_RESOURCES + "/textures/yellow_texture.png";
            material.tint = {1.0f, 0.84f, 0.18f, 1.0f};
            collectible.rotationSpeed = 1.2f;
        }
        else if (spawn.item.id == "health_potion")
        {
            transform.scale = {0.30f, 0.30f, 0.30f};
            material.texturePath = GraphicsConstants::ENGINE_RESOURCES + "/textures/red_texture.png";
            material.tint = {0.92f, 0.20f, 0.20f, 1.0f};
            collectible.rotationSpeed = 1.8f;
        }
        else
        {
            transform.scale = {0.35f, 0.35f, 0.35f};
            material.texturePath = GraphicsConstants::ENGINE_RESOURCES + "/textures/yellow_texture.png";
            material.tint = {1.0f, 0.82f, 0.18f, 1.0f};
            collectible.rotationSpeed = 2.2f;
        }

        PhysicsBodyComponent body;
        SphereColliderComponent collider;
        collider.radius = 0.35f;

        StaticMeshComponent mesh;
        mesh.meshPath = cubeMesh;

        BoundsComponent bounds;
        bounds.min = {-0.25f, -0.25f, -0.25f};
        bounds.max = { 0.25f,  0.25f,  0.25f};

        world.addComponent(entity, transform);
        world.addComponent(entity, body);
        world.addComponent(entity, collider);
        world.addComponent(entity, mesh);
        world.addComponent(entity, material);
        world.addComponent(entity, bounds);
        world.addComponent(entity, collectible);
    }
}

void CollectibleSpawner::chooseNewCollectibleRunState(const DungeonManager& dungeon,
                                                      CollectibleRunState& runState) const
{
    runState.collectibles.clear();

    const auto& floors = dungeon.getFloors();
    std::vector<std::pair<int, glm::ivec2>> keyCandidates;
    std::mt19937 rng(std::random_device{}());

    for (const GeneratedFloor& floor : floors)
    {
        std::vector<glm::ivec2> candidates;

        if (!floor.treasureSpawns.empty())
        {
            for (const glm::ivec2& treasure : floor.treasureSpawns)
                candidates.push_back(treasure);
        }
        else
        {
            for (int z = 0; z < floor.height; ++z)
            {
                for (int x = 0; x < floor.width; ++x)
                {
                    const glm::ivec2 tile = {x, z};
                    if (!floor.isWalkable(x, z)) continue;
                    if (tile == floor.playerStart) continue;
                    if (tile == floor.stairDownPos || tile == floor.stairUpPos) continue;
                    candidates.push_back(tile);
                }
            }
        }

        std::shuffle(candidates.begin(), candidates.end(), rng);
        for (const glm::ivec2& candidate : candidates)
            keyCandidates.push_back({floor.floorNumber, candidate});

        if (!candidates.empty())
        {
            runState.collectibles.push_back(CollectibleSpawnState{
                CollectibleType::InventoryItem,
                makeHealthPotionItem(),
                0,
                floor.floorNumber,
                candidates[0],
                false
            });
        }

        if (candidates.size() > 1)
        {
            runState.collectibles.push_back(CollectibleSpawnState{
                CollectibleType::Gold,
                {},
                25,
                floor.floorNumber,
                candidates[1],
                false
            });
        }

        for (size_t i = std::min<size_t>(2, candidates.size()); i < candidates.size(); ++i)
            keyCandidates.push_back({floor.floorNumber, candidates[i]});
    }

    if (keyCandidates.empty())
    {
        runState.collectibles.push_back(CollectibleSpawnState{
            CollectibleType::InventoryItem,
            makeGoldenKeyItem(),
            0,
            0,
            dungeon.getActiveFloor().playerStart,
            false
        });
        return;
    }

    std::shuffle(keyCandidates.begin(), keyCandidates.end(), rng);
    runState.collectibles.push_back(CollectibleSpawnState{
        CollectibleType::InventoryItem,
        makeGoldenKeyItem(),
        0,
        keyCandidates.front().first,
        keyCandidates.front().second,
        false
    });
}
