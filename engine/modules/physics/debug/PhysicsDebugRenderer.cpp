#include "PhysicsDebugRenderer.h"

#include <chrono>
#include <cmath>
#include <unordered_set>

#include "BoundsComponent.h"
#include "ECSWorld.hpp"
#include "EcsControllerContext.hpp"
#include "GraphicsConstants.h"
#include "MaterialComponent.h"
#include "PhysicsWorld.h"
#include "ProceduralMeshComponent.h"
#include "RenderGpuComponent.h"
#include "SphereColliderComponent.h"
#include "TransformComponent.h"
#include "TransformDirtyHelpers.h"

namespace
{
    constexpr float LINE_THICKNESS = 0.03f;
    const glm::vec4 DEBUG_TINT     = {0.15f, 0.95f, 0.95f, 0.8f};

    Entity createSegmentEntity(ECSWorld& world)
    {
        Entity e = world.createEntity();

        ProceduralMeshComponent mesh;
        mesh.width  = 1.0f;
        mesh.height = LINE_THICKNESS;

        MaterialComponent material;
        material.texturePath = GraphicsConstants::ENGINE_RESOURCES + "/textures/cyan_texture.png";
        material.tint        = DEBUG_TINT;
        material.alpha       = DEBUG_TINT.a;
        material.doubleSided = true;

        TransformComponent transform;

        BoundsComponent bounds;
        bounds.min = {-0.5f, -LINE_THICKNESS * 0.5f, -LINE_THICKNESS * 0.5f};
        bounds.max = { 0.5f,  LINE_THICKNESS * 0.5f,  LINE_THICKNESS * 0.5f};

        world.addComponent(e, mesh);
        world.addComponent(e, material);
        world.addComponent(e, transform);
        world.addComponent(e, bounds);
        return e;
    }

    void releaseSegmentEntity(ECSWorld& world, Entity entity)
    {
        world.destroyEntity(entity);
    }

    void updateSegment(ECSWorld& world,
                       Entity entity,
                       const glm::vec3& position,
                       const glm::vec3& rotation,
                       float length)
    {
        auto& transform = world.getComponent<TransformComponent>(entity);
        transform.position = position;
        transform.rotation = rotation;
        gts::transform::markDirty(world, entity);

        const auto& mesh = world.getComponent<ProceduralMeshComponent>(entity);
        if (mesh.width != length)
        {
            ProceduralMeshComponent updatedMesh = mesh;
            updatedMesh.width = length;
            world.addComponent<ProceduralMeshComponent>(entity, updatedMesh);
        }

        auto& bounds = world.getComponent<BoundsComponent>(entity);
        bounds.min = {-length * 0.5f, -LINE_THICKNESS * 0.5f, -LINE_THICKNESS * 0.5f};
        bounds.max = { length * 0.5f,  LINE_THICKNESS * 0.5f,  LINE_THICKNESS * 0.5f};

        if (world.hasComponent<RenderGpuComponent>(entity))
            world.getComponent<RenderGpuComponent>(entity).dirty = true;
    }

    void updateRing(ECSWorld& world,
                    const std::vector<Entity>& debugEntities,
                    size_t segmentStart,
                    const glm::vec3& center,
                    float radius,
                    const glm::vec3& baseRotation)
    {
        constexpr float TWO_PI = glm::two_pi<float>();

        for (int i = 0; i < PhysicsDebugRenderer::SEGMENTS_PER_RING; ++i)
        {
            const float a0 = TWO_PI * static_cast<float>(i) / static_cast<float>(PhysicsDebugRenderer::SEGMENTS_PER_RING);
            const float a1 = TWO_PI * static_cast<float>(i + 1) / static_cast<float>(PhysicsDebugRenderer::SEGMENTS_PER_RING);

            const glm::vec2 p0 = {std::cos(a0) * radius, std::sin(a0) * radius};
            const glm::vec2 p1 = {std::cos(a1) * radius, std::sin(a1) * radius};
            const glm::vec2 mid = (p0 + p1) * 0.5f;
            const glm::vec2 delta = p1 - p0;
            const float length = glm::length(delta);
            const float angle = std::atan2(delta.y, delta.x);

            glm::vec3 rotation = baseRotation;
            rotation.z += angle;

            glm::vec3 segmentPosition = center;
            if (baseRotation.x == 0.0f && baseRotation.y == 0.0f)
                segmentPosition += glm::vec3(mid.x, mid.y, 0.0f);
            else if (baseRotation.x != 0.0f)
                segmentPosition += glm::vec3(mid.x, 0.0f, mid.y);
            else
                segmentPosition += glm::vec3(0.0f, mid.y, mid.x);

            updateSegment(world,
                          debugEntities[segmentStart + static_cast<size_t>(i)],
                          segmentPosition,
                          rotation,
                          length);
        }
    }
}

void PhysicsDebugRenderer::update(const EcsControllerContext& ctx)
{
    if (!enabled) return;

    using Clock = std::chrono::steady_clock;
    const auto debugStart = Clock::now();

    std::unordered_set<entity_id_type> liveColliderIds;

    ctx.world.forEach<TransformComponent, SphereColliderComponent>(
        [&](Entity colliderEntity, TransformComponent& transform, SphereColliderComponent& collider)
    {
        liveColliderIds.insert(colliderEntity.id);

        auto& debugEntities = debugEntitiesByCollider[colliderEntity.id];
        if (debugEntities.empty())
        {
            debugEntities.reserve(SEGMENTS_PER_COLLIDER);
            for (int i = 0; i < SEGMENTS_PER_COLLIDER; ++i)
                debugEntities.push_back(createSegmentEntity(ctx.world));
        }

        updateRing(ctx.world, debugEntities, 0, transform.position, collider.radius, {0.0f, 0.0f, 0.0f});
        updateRing(ctx.world, debugEntities, SEGMENTS_PER_RING, transform.position, collider.radius, {-glm::half_pi<float>(), 0.0f, 0.0f});
        updateRing(ctx.world, debugEntities, SEGMENTS_PER_RING * 2, transform.position, collider.radius, {0.0f, glm::half_pi<float>(), 0.0f});
    });

    std::vector<entity_id_type> staleColliderIds;
    staleColliderIds.reserve(debugEntitiesByCollider.size());

    size_t debugSegmentCount = 0;
    for (const auto& [colliderId, debugEntities] : debugEntitiesByCollider)
    {
        if (!liveColliderIds.contains(colliderId))
        {
            for (Entity entity : debugEntities)
                releaseSegmentEntity(ctx.world, entity);
            staleColliderIds.push_back(colliderId);
            continue;
        }

        debugSegmentCount += debugEntities.size();
    }

    for (entity_id_type colliderId : staleColliderIds)
        debugEntitiesByCollider.erase(colliderId);

    if (auto* physicsWorld = dynamic_cast<PhysicsWorld*>(ctx.physics))
    {
        const auto debugEnd = Clock::now();
        const double debugMs = std::chrono::duration<double, std::milli>(debugEnd - debugStart).count();
        physicsWorld->setDebugProfileStats(liveColliderIds.size(), debugSegmentCount, debugMs);
        physicsWorld->maybePrintProfile();
    }
}
