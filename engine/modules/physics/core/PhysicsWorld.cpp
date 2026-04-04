#include "PhysicsWorld.h"

PhysicsWorld::PhysicsWorld(ECSWorld* world)
    : worldRef(world)
{
}

void PhysicsWorld::update(float dt)
{
    lastDt = dt;
    clearCollisions();
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
