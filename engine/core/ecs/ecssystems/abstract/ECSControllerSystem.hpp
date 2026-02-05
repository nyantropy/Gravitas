#pragma once
#include "ECSWorld.hpp"
#include "SceneContext.h"

class ECSControllerSystem 
{
    public:
        virtual ~ECSControllerSystem() = default;
        virtual void update(class ECSWorld& world, SceneContext& ctx) = 0;
};