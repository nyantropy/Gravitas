#include "PhysicsSystem.h"

#include <chrono>
#include <cmath>
#include <utility>
#include <vector>

#include "ECSWorld.hpp"
#include "EcsSimulationContext.hpp"
#include "PhysicsWorld.h"
#include "PhysicsBodyComponent.h"
#include "SphereColliderComponent.h"
#include "TransformComponent.h"

void PhysicsSystem::update(const EcsSimulationContext& ctx)
{
    if (!physicsWorld) return;

    using Clock = std::chrono::steady_clock;

    physicsWorld->setWorld(&ctx.world);
    physicsWorld->update(ctx.dt);

    PhysicsWorld::ProfileStats stats = physicsWorld->getProfileStats();
    stats.totalEntities = ctx.world.getEntityCount();

    const auto collectionStart = Clock::now();
    const auto entities = ctx.world.getAllEntitiesWith<TransformComponent,
                                                       PhysicsBodyComponent,
                                                       SphereColliderComponent>();
    const auto collectionEnd = Clock::now();
    stats.physicsEntities = entities.size();
    stats.colliderCount   = entities.size();
    stats.collectionMs    = std::chrono::duration<double, std::milli>(collectionEnd - collectionStart).count();

    std::vector<std::pair<size_t, size_t>> candidatePairs;
    candidatePairs.reserve(entities.size() > 1 ? (entities.size() * (entities.size() - 1)) / 2 : 0);

    const auto broadStart = Clock::now();

    for (size_t i = 0; i < entities.size(); ++i)
    {
        const Entity a = entities[i];
        const auto& transformA = ctx.world.getComponent<TransformComponent>(a);
        const auto& bodyA = ctx.world.getComponent<PhysicsBodyComponent>(a);
        const auto& colliderA = ctx.world.getComponent<SphereColliderComponent>(a);

        for (size_t j = i + 1; j < entities.size(); ++j)
        {
            ++stats.broadPhaseChecks;

            const Entity b = entities[j];
            const auto& transformB = ctx.world.getComponent<TransformComponent>(b);
            const auto& bodyB = ctx.world.getComponent<PhysicsBodyComponent>(b);
            const auto& colliderB = ctx.world.getComponent<SphereColliderComponent>(b);

            if (!bodyA.dynamic && !bodyB.dynamic)
                continue;

            const float combinedRadius = colliderA.radius + colliderB.radius;
            const glm::vec3 delta = transformB.position - transformA.position;

            // Broad phase: reject obvious misses using per-axis overlap.
            if (std::abs(delta.x) > combinedRadius) continue;
            if (std::abs(delta.y) > combinedRadius) continue;
            if (std::abs(delta.z) > combinedRadius) continue;
            candidatePairs.emplace_back(i, j);
        }
    }

    const auto broadEnd = Clock::now();
    stats.broadPhaseCandidates = candidatePairs.size();
    stats.broadPhaseMs = std::chrono::duration<double, std::milli>(broadEnd - broadStart).count();

    const auto narrowStart = Clock::now();
    for (const auto& [i, j] : candidatePairs)
    {
        const Entity a = entities[i];
        const Entity b = entities[j];
        const auto& transformA = ctx.world.getComponent<TransformComponent>(a);
        const auto& transformB = ctx.world.getComponent<TransformComponent>(b);
        const auto& colliderA = ctx.world.getComponent<SphereColliderComponent>(a);
        const auto& colliderB = ctx.world.getComponent<SphereColliderComponent>(b);

        const float combinedRadius = colliderA.radius + colliderB.radius;
        const glm::vec3 delta = transformB.position - transformA.position;

        const float distanceSquared = glm::dot(delta, delta);
        if (distanceSquared > combinedRadius * combinedRadius)
            continue;

        physicsWorld->recordCollision(a, b);
    }
    const auto narrowEnd = Clock::now();
    stats.collisionsDetected = physicsWorld->getCollisions().size();
    stats.narrowPhaseMs = std::chrono::duration<double, std::milli>(narrowEnd - narrowStart).count();

    physicsWorld->setProfileStats(stats);
    physicsWorld->maybePrintProfile();
}
