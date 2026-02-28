#pragma once
#include "ECSWorld.hpp"

// a pure system, best suited for actual game simulation
class ECSSimulationSystem 
{
    public:
        virtual ~ECSSimulationSystem() = default;
        virtual void update(class ECSWorld& world, float dt) = 0;
};