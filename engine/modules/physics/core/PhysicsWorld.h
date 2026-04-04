#pragma once

#include <cstdint>
#include <vector>

#include "CollisionEvent.h"
#include "IGtsPhysicsModule.h"

class ECSWorld;

class PhysicsWorld : public IGtsPhysicsModule
{
public:
    struct ProfileStats
    {
        uint64_t frameIndex           = 0;
        uint64_t simulationUpdates    = 0;
        uint64_t debugUpdates         = 0;
        size_t   totalEntities        = 0;
        size_t   physicsEntities      = 0;
        size_t   colliderCount        = 0;
        size_t   broadPhaseChecks     = 0;
        size_t   broadPhaseCandidates = 0;
        size_t   collisionsDetected   = 0;
        size_t   debugColliderCount   = 0;
        size_t   debugSegmentCount    = 0;
        double   collectionMs         = 0.0;
        double   broadPhaseMs         = 0.0;
        double   narrowPhaseMs        = 0.0;
        double   debugRenderMs        = 0.0;
    };

    explicit PhysicsWorld(ECSWorld* world = nullptr);

    void update(float dt) override;
    const std::vector<CollisionEvent>& getCollisions() const override;
    void clearCollisions() override;

    void setWorld(ECSWorld* world);
    ECSWorld* getWorld() const;

    void recordCollision(Entity a, Entity b);
    void setProfileStats(const ProfileStats& stats);
    const ProfileStats& getProfileStats() const;
    void setDebugProfileStats(size_t colliderCount, size_t segmentCount, double debugRenderMs);
    void maybePrintProfile();

private:
    ECSWorld*                    worldRef = nullptr;
    std::vector<CollisionEvent>  collisions;
    float                        lastDt = 0.0f;
    ProfileStats                 profileStats;
};
