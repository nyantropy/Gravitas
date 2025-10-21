#pragma once
#include "ECSWorld.hpp"

class System 
{
    public:
        virtual ~System() = default;
        virtual void update(float deltaTime, class ECSWorld& world) = 0;
};