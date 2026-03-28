#pragma once
#include "ECSWorld.hpp"
#include "SceneContext.h"

// a more open system that can access the scene context - a "bridge" of sorts
class ECSControllerSystem 
{
    public:
        virtual ~ECSControllerSystem() = default;
        virtual void update(class ECSWorld& world, SceneContext& ctx) = 0;
};