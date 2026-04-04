#include "PhysicsWorld.h"

#include <cstdio>

PhysicsWorld::PhysicsWorld(ECSWorld* world)
    : worldRef(world)
{
}

void PhysicsWorld::update(float dt)
{
    lastDt = dt;
    clearCollisions();
    ++profileStats.frameIndex;
    ++profileStats.simulationUpdates;
}

const std::vector<CollisionEvent>& PhysicsWorld::getCollisions() const
{
    return collisions;
}

void PhysicsWorld::clearCollisions()
{
    collisions.clear();
}

void PhysicsWorld::setWorld(ECSWorld* world)
{
    worldRef = world;
}

ECSWorld* PhysicsWorld::getWorld() const
{
    return worldRef;
}

void PhysicsWorld::recordCollision(Entity a, Entity b)
{
    collisions.push_back({a, b});
}

void PhysicsWorld::setProfileStats(const ProfileStats& stats)
{
    profileStats = stats;
}

const PhysicsWorld::ProfileStats& PhysicsWorld::getProfileStats() const
{
    return profileStats;
}

void PhysicsWorld::setDebugProfileStats(size_t colliderCount, size_t segmentCount, double debugRenderMs)
{
    ++profileStats.debugUpdates;
    profileStats.debugColliderCount = colliderCount;
    profileStats.debugSegmentCount  = segmentCount;
    profileStats.debugRenderMs      = debugRenderMs;
}

void PhysicsWorld::maybePrintProfile()
{
    if (profileStats.frameIndex == 0 || (profileStats.frameIndex % 60u) != 0u)
        return;

    std::printf(
        "[physics] frame=%llu sim=%llu debug=%llu entities=%zu physics=%zu colliders=%zu "
        "checks=%zu candidates=%zu collisions=%zu debugColliders=%zu debugSegments=%zu "
        "collection=%.3fms broad=%.3fms narrow=%.3fms debug=%.3fms\n",
        static_cast<unsigned long long>(profileStats.frameIndex),
        static_cast<unsigned long long>(profileStats.simulationUpdates),
        static_cast<unsigned long long>(profileStats.debugUpdates),
        profileStats.totalEntities,
        profileStats.physicsEntities,
        profileStats.colliderCount,
        profileStats.broadPhaseChecks,
        profileStats.broadPhaseCandidates,
        profileStats.collisionsDetected,
        profileStats.debugColliderCount,
        profileStats.debugSegmentCount,
        profileStats.collectionMs,
        profileStats.broadPhaseMs,
        profileStats.narrowPhaseMs,
        profileStats.debugRenderMs);
}
