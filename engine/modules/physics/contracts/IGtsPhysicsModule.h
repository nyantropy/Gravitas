#pragma once

#include <vector>

struct CollisionEvent;

class IGtsPhysicsModule
{
public:
    virtual ~IGtsPhysicsModule() = default;

    virtual void update(float dt) = 0;
    virtual const std::vector<CollisionEvent>& getCollisions() const = 0;
    virtual void clearCollisions() = 0;
};
