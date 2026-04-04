#pragma once

#include <vector>

#include "CollisionEvent.h"
#include "IGtsPhysicsModule.h"

class ECSWorld;

class PhysicsWorld : public IGtsPhysicsModule
{
public:
    explicit PhysicsWorld(ECSWorld* world = nullptr);

    void update(float dt) override;
    const std::vector<CollisionEvent>& getCollisions() const override;
    void clearCollisions() override;

    void setWorld(ECSWorld* world);
    ECSWorld* getWorld() const;

    void recordCollision(Entity a, Entity b);

private:
    ECSWorld*                    worldRef = nullptr;
    std::vector<CollisionEvent>  collisions;
    float                        lastDt = 0.0f;
};
