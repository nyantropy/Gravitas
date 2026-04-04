#include "PhysicsSystem.h"

#include <cmath>

#include "ECSWorld.hpp"
#include "PhysicsWorld.h"
#include "PhysicsBodyComponent.h"
#include "SphereColliderComponent.h"
#include "TransformComponent.h"

void PhysicsSystem::update(ECSWorld& world, float dt)
{
    if (!physicsWorld) return;

    physicsWorld->setWorld(&world);
    physicsWorld->update(dt);

    const auto entities = world.getAllEntitiesWith<TransformComponent,
                                                   PhysicsBodyComponent,
                                                   SphereColliderComponent>();

    for (size_t i = 0; i < entities.size(); ++i)
    {
        const Entity a = entities[i];
        const auto& transformA = world.getComponent<TransformComponent>(a);
        const auto& bodyA = world.getComponent<PhysicsBodyComponent>(a);
        const auto& colliderA = world.getComponent<SphereColliderComponent>(a);

        for (size_t j = i + 1; j < entities.size(); ++j)
        {
            const Entity b = entities[j];
            const auto& transformB = world.getComponent<TransformComponent>(b);
            const auto& bodyB = world.getComponent<PhysicsBodyComponent>(b);
            const auto& colliderB = world.getComponent<SphereColliderComponent>(b);

            if (!bodyA.dynamic && !bodyB.dynamic)
                continue;

            const float combinedRadius = colliderA.radius + colliderB.radius;
            const glm::vec3 delta = transformB.position - transformA.position;

            // Broad phase: reject obvious misses using per-axis overlap.
            if (std::abs(delta.x) > combinedRadius) continue;
            if (std::abs(delta.y) > combinedRadius) continue;
            if (std::abs(delta.z) > combinedRadius) continue;

            // Narrow phase: sphere-vs-sphere distance check.
            const float distanceSquared = glm::dot(delta, delta);
            if (distanceSquared > combinedRadius * combinedRadius)
                continue;

            physicsWorld->recordCollision(a, b);
        }
    }
}
