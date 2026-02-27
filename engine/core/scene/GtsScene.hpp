#pragma once

#include "ECSWorld.hpp"
#include "SceneContext.h"

#include "RenderGpuSystem.hpp"
#include "CameraGpuSystem.hpp"
#include "RenderBindingSystem.hpp"
#include "CameraBindingSystem.hpp"
#include "RenderResourceClearSystem.hpp"


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
        virtual void onUpdate(SceneContext& ctx) = 0;

        ECSWorld& getWorld()
        {
            return ecsWorld;
        }

        // pre defined rendering systems - should be called once in the onLoad() function of whatever scene you design
        inline void installRendererFeature()
        {
            ecsWorld.addSimulationSystem<RenderGpuSystem>();
            ecsWorld.addSimulationSystem<CameraGpuSystem>();
            ecsWorld.addControllerSystem<RenderBindingSystem>();
            ecsWorld.addControllerSystem<CameraBindingSystem>();
            ecsWorld.addControllerSystem<RenderResourceClearSystem>();
        }
};
