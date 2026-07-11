#include "PhysicsDebugRenderer.h"

#include <chrono>
#include <cmath>
#include <cstddef>

#include "DebugDrawPrimitives.h"
#include "ECSWorld.hpp"
#include "EcsControllerContext.hpp"
#include "PhysicsWorld.h"
#include "SphereColliderComponent.h"
#include "TransformComponent.h"

namespace
{
    constexpr float LineThickness = 0.030f;
    constexpr gts::debugdraw::DebugDrawColor ColliderColor = gts::debugdraw::DebugDrawColor::Cyan;

    enum class RingPlane
    {
        XY,
        XZ,
        YZ
    };

    glm::vec3 ringPoint(const glm::vec3& center,
                        float radius,
                        float angle,
                        RingPlane plane)
    {
        const float x = std::cos(angle) * radius;
        const float y = std::sin(angle) * radius;

        switch (plane)
        {
            case RingPlane::XY:
                return center + glm::vec3(x, y, 0.0f);
            case RingPlane::XZ:
                return center + glm::vec3(x, 0.0f, y);
            case RingPlane::YZ:
                return center + glm::vec3(0.0f, y, x);
        }

        return center;
    }

    size_t appendRing(gts::debugdraw::DebugDrawQueueComponent& queue,
                      const glm::vec3& center,
                      float radius,
                      RingPlane plane)
    {
        if (radius <= 0.0001f)
            return 0;

        constexpr float TwoPi = 6.28318530717958647692f;
        size_t lineCount = 0;
        for (int i = 0; i < PhysicsDebugRenderer::SEGMENTS_PER_RING; ++i)
        {
            const float a0 = TwoPi * static_cast<float>(i) /
                static_cast<float>(PhysicsDebugRenderer::SEGMENTS_PER_RING);
            const float a1 = TwoPi * static_cast<float>(i + 1) /
                static_cast<float>(PhysicsDebugRenderer::SEGMENTS_PER_RING);
            gts::debugdraw::line(queue,
                                 ringPoint(center, radius, a0, plane),
                                 ringPoint(center, radius, a1, plane),
                                 ColliderColor,
                                 LineThickness);
            ++lineCount;
        }
        return lineCount;
    }

    size_t appendSphereCollider(gts::debugdraw::DebugDrawQueueComponent& queue,
                                const glm::vec3& center,
                                float radius)
    {
        return appendRing(queue, center, radius, RingPlane::XY) +
            appendRing(queue, center, radius, RingPlane::XZ) +
            appendRing(queue, center, radius, RingPlane::YZ);
    }
}

void PhysicsDebugRenderer::update(const EcsControllerContext& ctx)
{
    if (!enabled)
        return;

    using Clock = std::chrono::steady_clock;
    const auto debugStart = Clock::now();

    gts::debugdraw::DebugDrawQueueComponent& queue = gts::debugdraw::ensureQueue(ctx.world);
    size_t colliderCount = 0;
    size_t debugSegmentCount = 0;

    ctx.world.forEach<TransformComponent, SphereColliderComponent>(
        [&](Entity, TransformComponent& transform, SphereColliderComponent& collider)
    {
        ++colliderCount;
        debugSegmentCount += appendSphereCollider(queue, transform.position, collider.radius);
    });

    if (auto* physicsWorld = dynamic_cast<PhysicsWorld*>(ctx.physics))
    {
        const auto debugEnd = Clock::now();
        const double debugMs = std::chrono::duration<double, std::milli>(debugEnd - debugStart).count();
        physicsWorld->setDebugProfileStats(colliderCount, debugSegmentCount, debugMs);
        physicsWorld->maybePrintProfile();
    }
}
