#pragma once

#include "ECSWorld.hpp"
#include "SceneContext.h"

// core idea: override this class in whatever application you are making, and create your own scene using entities and components
class GtsScene 
{
    protected:
        ECSWorld ecsWorld;
    public:
        virtual ~GtsScene() = default;

        // call this once whenever the scene is loaded
        virtual void onLoad(SceneContext& ctx) = 0;

        // call this every time the scene is updated
        virtual void onUpdate(SceneContext& ctx, float dt) = 0;

        ECSWorld& getWorld() 
        {
            return ecsWorld;
        }
};