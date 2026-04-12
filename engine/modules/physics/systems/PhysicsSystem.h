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

    void update(const EcsSimulationContext& ctx) override;

private:
    PhysicsWorld* physicsWorld = nullptr;
};
