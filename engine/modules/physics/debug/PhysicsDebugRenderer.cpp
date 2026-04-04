#include "PhysicsDebugRenderer.h"

#include <array>
#include <cmath>

#include "BoundsComponent.h"
#include "ECSWorld.hpp"
#include "GraphicsConstants.h"
#include "MaterialComponent.h"
#include "ProceduralMeshComponent.h"
#include "RenderGpuComponent.h"
#include "RenderResourceClearComponent.h"
#include "SphereColliderComponent.h"
#include "TransformComponent.h"

namespace
{
    constexpr int   RING_SEGMENTS   = 12;
    constexpr float LINE_THICKNESS  = 0.03f;
    const glm::vec4 DEBUG_TINT      = {0.15f, 0.95f, 0.95f, 0.8f};

    void clearDebugEntities(ECSWorld& world, std::vector<Entity>& debugEntities)
    {
        for (Entity entity : debugEntities)
        {
            if (world.hasComponent<RenderGpuComponent>(entity)
                && !world.hasComponent<RenderResourceClearComponent>(entity))
            {
                world.addComponent(entity, RenderResourceClearComponent{});
            }
            else if (!world.hasComponent<RenderGpuComponent>(entity))
            {
                world.destroyEntity(entity);
            }
        }

        debugEntities.clear();
    }

    void spawnSegment(ECSWorld& world,
                      std::vector<Entity>& debugEntities,
                      const glm::vec3& position,
                      const glm::vec3& rotation,
                      float length)
    {
        Entity e = world.createEntity();
        debugEntities.push_back(e);

        ProceduralMeshComponent mesh;
        mesh.width  = length;
        mesh.height = LINE_THICKNESS;

        MaterialComponent material;
        material.texturePath = GraphicsConstants::ENGINE_RESOURCES + "/textures/cyan_texture.png";
        material.tint        = DEBUG_TINT;
        material.alpha       = DEBUG_TINT.a;
        material.doubleSided = true;

        TransformComponent transform;
        transform.position = position;
        transform.rotation = rotation;

        BoundsComponent bounds;
        bounds.min = {-length * 0.5f, -LINE_THICKNESS * 0.5f, -LINE_THICKNESS * 0.5f};
        bounds.max = { length * 0.5f,  LINE_THICKNESS * 0.5f,  LINE_THICKNESS * 0.5f};

        world.addComponent(e, mesh);
        world.addComponent(e, material);
        world.addComponent(e, transform);
        world.addComponent(e, bounds);
    }

    void spawnRing(ECSWorld& world,
                   std::vector<Entity>& debugEntities,
                   const glm::vec3& center,
                   float radius,
                   const glm::vec3& baseRotation)
    {
        constexpr float TWO_PI = glm::two_pi<float>();

        for (int i = 0; i < RING_SEGMENTS; ++i)
        {
            const float a0 = TWO_PI * static_cast<float>(i) / static_cast<float>(RING_SEGMENTS);
            const float a1 = TWO_PI * static_cast<float>(i + 1) / static_cast<float>(RING_SEGMENTS);

            const glm::vec2 p0 = {std::cos(a0) * radius, std::sin(a0) * radius};
            const glm::vec2 p1 = {std::cos(a1) * radius, std::sin(a1) * radius};
            const glm::vec2 mid = (p0 + p1) * 0.5f;
            const glm::vec2 delta = p1 - p0;
            const float length = glm::length(delta);
            const float angle = std::atan2(delta.y, delta.x);

            TransformComponent temp;
            temp.rotation = baseRotation;
            temp.rotation.z += angle;

            glm::vec3 segmentPosition = center;
            if (baseRotation.x == 0.0f && baseRotation.y == 0.0f)
                segmentPosition += glm::vec3(mid.x, mid.y, 0.0f);
            else if (baseRotation.x != 0.0f)
                segmentPosition += glm::vec3(mid.x, 0.0f, mid.y);
            else
                segmentPosition += glm::vec3(0.0f, mid.y, mid.x);

            spawnSegment(world, debugEntities, segmentPosition, temp.rotation, length);
        }
    }
}

void PhysicsDebugRenderer::update(ECSWorld& world, SceneContext& /*ctx*/)
{
    clearDebugEntities(world, debugEntities);

    if (!enabled) return;

    world.forEach<TransformComponent, SphereColliderComponent>(
        [&](Entity, TransformComponent& transform, SphereColliderComponent& collider)
    {
        spawnRing(world, debugEntities, transform.position, collider.radius, {0.0f, 0.0f, 0.0f});
        spawnRing(world, debugEntities, transform.position, collider.radius, {-glm::half_pi<float>(), 0.0f, 0.0f});
        spawnRing(world, debugEntities, transform.position, collider.radius, {0.0f, glm::half_pi<float>(), 0.0f});
    });
}
