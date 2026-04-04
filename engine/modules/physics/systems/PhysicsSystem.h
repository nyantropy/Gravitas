#pragma once

#include "ECSSimulationSystem.hpp"

class PhysicsWorld;

class PhysicsSystem : public ECSSimulationSystem
{
public:
    explicit PhysicsSystem(PhysicsWorld* physicsWorld)
        : physicsWorld(physicsWorld)
    {
    }

    void update(ECSWorld& world, float dt) override;

private:
    PhysicsWorld* physicsWorld = nullptr;
};
