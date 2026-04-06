#include "dungeon/DungeonSpawner.h"

#include <string>

#include "BoundsComponent.h"
#include "GlmConfig.h"
#include "GraphicsConstants.h"
#include "MaterialComponent.h"
#include "ProceduralMeshComponent.h"
#include "TransformComponent.h"
#include "ECSWorld.hpp"

namespace
{
    constexpr float TEST_WALL_HEIGHT = 2.25f;
    constexpr float STAIR_STEP_DROP = 0.22f;
    constexpr float STAIR_STEP_INSET = 0.14f;

    glm::vec4 tintForTile(TileType tileType)
    {
        switch (tileType)
        {
            case TileType::StairUp:    return {0.20f, 0.80f, 0.25f, 1.0f};
            case TileType::StairDown:  return {0.85f, 0.20f, 0.20f, 1.0f};
            case TileType::Treasure:   return {0.95f, 0.82f, 0.18f, 1.0f};
            case TileType::EnemySpawn: return {0.62f, 0.30f, 0.75f, 1.0f};
            case TileType::Wall:       return {0.18f, 0.18f, 0.22f, 1.0f};
            case TileType::Floor:      return {0.60f, 0.60f, 0.60f, 1.0f};
        }

        return {1.0f, 1.0f, 1.0f, 1.0f};
    }

    std::string textureForTile(TileType tileType)
    {
        const std::string base = GraphicsConstants::ENGINE_RESOURCES + "/textures/";

        switch (tileType)
        {
            case TileType::StairUp:
                return base + "green_texture.png";
            case TileType::StairDown:
                return base + "orange_texture.png";
            default:
                return base + "grey_texture.png";
        }
    }

    bool isWalkable(const GeneratedFloor& floor, int x, int z)
    {
        return floor.inBounds(x, z) && floor.isWalkable(x, z);
    }

    glm::ivec2 findWalkableNeighborDirection(const GeneratedFloor& floor, const glm::ivec2& tilePos)
    {
        static constexpr glm::ivec2 directions[] = {
            {0, -1},
            {1, 0},
            {0, 1},
            {-1, 0},
        };

        for (const glm::ivec2& dir : directions)
        {
            const glm::ivec2 candidate = tilePos + dir;
            if (floor.isWalkable(candidate.x, candidate.y))
                return dir;
        }

        return {0, -1};
    }

    void spawnWallSegment(ECSWorld& world,
                          std::vector<Entity>& floorEntities,
                          float x,
                          float y,
                          float z,
                          float yawRadians)
    {
        Entity entity = world.createEntity();
        floorEntities.push_back(entity);

        ProceduralMeshComponent mesh;
        mesh.width  = 1.0f;
        mesh.height = 1.0f;

        MaterialComponent material;
        material.texturePath = GraphicsConstants::ENGINE_RESOURCES + "/textures/grey_texture.png";
        material.tint        = tintForTile(TileType::Wall);
        material.doubleSided = false;

        TransformComponent transform;
        transform.position   = {x, y, z};
        transform.rotation.y = yawRadians;
        transform.scale      = {1.0f, TEST_WALL_HEIGHT, 1.0f};

        BoundsComponent bounds;
        bounds.min = {-0.5f, -0.5f * TEST_WALL_HEIGHT, -0.05f};
        bounds.max = { 0.5f,  0.5f * TEST_WALL_HEIGHT,  0.05f};

        world.addComponent(entity, transform);
        world.addComponent(entity, mesh);
        world.addComponent(entity, material);
        world.addComponent(entity, bounds);
    }
}

void DungeonSpawner::buildFloor(ECSWorld& world,
                                std::vector<Entity>& floorEntities,
                                const GeneratedFloor& floor) const
{
    for (int z = 0; z < floor.height; ++z)
    {
        for (int x = 0; x < floor.width; ++x)
        {
            const TileType tileType = floor.get(x, z);
            if (tileType != TileType::Wall)
            {
                Entity entity = world.createEntity();
                floorEntities.push_back(entity);

                ProceduralMeshComponent mesh;
                MaterialComponent       material;
                TransformComponent      transform;
                BoundsComponent         bounds;

                material.texturePath = textureForTile(tileType);
                material.tint        = tintForTile(tileType);
                material.doubleSided = true;

                mesh.width          = 1.0f;
                mesh.height         = 1.0f;
                transform.position  = {x + 0.5f, 0.0f, z + 0.5f};
                transform.rotation.x = -glm::half_pi<float>();
                bounds.min          = {-0.5f, -0.5f, -0.02f};
                bounds.max          = { 0.5f,  0.5f,  0.02f};

                world.addComponent(entity, transform);
                world.addComponent(entity, mesh);
                world.addComponent(entity, material);
                world.addComponent(entity, bounds);

                if (tileType == TileType::StairDown || tileType == TileType::StairUp)
                    spawnStairFeature(world, floorEntities, floor, {x, z}, tileType == TileType::StairDown);
            }
            else
            {
                if (isWalkable(floor, x, z - 1))
                    spawnWallSegment(world, floorEntities,
                                     x + 0.5f, TEST_WALL_HEIGHT * 0.5f, static_cast<float>(z),
                                     glm::pi<float>());

                if (isWalkable(floor, x, z + 1))
                    spawnWallSegment(world, floorEntities,
                                     x + 0.5f, TEST_WALL_HEIGHT * 0.5f, z + 1.0f,
                                     0.0f);

                if (isWalkable(floor, x + 1, z))
                    spawnWallSegment(world, floorEntities,
                                     x + 1.0f, TEST_WALL_HEIGHT * 0.5f, z + 0.5f,
                                     -glm::half_pi<float>());

                if (isWalkable(floor, x - 1, z))
                    spawnWallSegment(world, floorEntities,
                                     static_cast<float>(x), TEST_WALL_HEIGHT * 0.5f, z + 0.5f,
                                     glm::half_pi<float>());
            }
        }
    }
}

void DungeonSpawner::destroyFloor(ECSWorld& world, std::vector<Entity>& floorEntities) const
{
    for (Entity entity : floorEntities)
        world.destroyEntity(entity);

    floorEntities.clear();
}

void DungeonSpawner::spawnStairFeature(ECSWorld& world,
                                       std::vector<Entity>& floorEntities,
                                       const GeneratedFloor& floor,
                                       const glm::ivec2& stairPos,
                                       bool descends) const
{
    const TileType stairTile = descends ? TileType::StairDown : TileType::StairUp;
    const glm::ivec2 direction = findWalkableNeighborDirection(floor, stairPos);
    const glm::vec2 dirVec = glm::normalize(glm::vec2(static_cast<float>(direction.x),
                                                      static_cast<float>(direction.y)));

    for (int step = 0; step < 3; ++step)
    {
        Entity entity = world.createEntity();
        floorEntities.push_back(entity);

        const float stepFactor = static_cast<float>(step + 1) / 3.0f;
        const float width = 1.0f - STAIR_STEP_INSET * 2.0f * stepFactor;
        const float depth = 1.0f - STAIR_STEP_INSET * 1.5f * stepFactor;
        const float yOffset = descends
            ? -STAIR_STEP_DROP * stepFactor
            : STAIR_STEP_DROP * (1.0f - stepFactor) * 0.75f;
        const glm::vec2 slide = dirVec * (0.12f * (stepFactor - 0.5f));

        ProceduralMeshComponent mesh;
        mesh.width = width;
        mesh.height = depth;

        MaterialComponent material;
        material.texturePath = textureForTile(stairTile);
        material.tint = tintForTile(stairTile);
        material.doubleSided = true;

        TransformComponent transform;
        transform.position = {
            stairPos.x + 0.5f + slide.x,
            yOffset,
            stairPos.y + 0.5f + slide.y
        };
        transform.rotation.x = -glm::half_pi<float>();

        BoundsComponent bounds;
        bounds.min = {-width * 0.5f, -depth * 0.5f, -0.02f};
        bounds.max = { width * 0.5f,  depth * 0.5f,  0.02f};

        world.addComponent(entity, transform);
        world.addComponent(entity, mesh);
        world.addComponent(entity, material);
        world.addComponent(entity, bounds);
    }
}
