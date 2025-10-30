#pragma once
#include "ECSWorld.hpp"

class ECSSystem 
{
    public:
        virtual ~ECSSystem() = default;
        virtual void update(class ECSWorld& world, float dt) = 0;
};