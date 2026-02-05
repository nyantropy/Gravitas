#pragma once
#include "ECSWorld.hpp"

class ECSSimulationSystem 
{
    public:
        virtual ~ECSSimulationSystem() = default;
        virtual void update(class ECSWorld& world, float dt) = 0;
};