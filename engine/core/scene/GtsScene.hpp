#pragma once

#include "ECSWorld.hpp"
#include "SceneContext.h"
#include "GtsSceneTransitionData.h"

#include "RenderGpuSystem.hpp"
#include "CameraGpuSystem.hpp"
#include "RenderBindingSystem.hpp"
#include "CameraBindingSystem.hpp"
#include "RenderResourceClearSystem.hpp"
#include "DefaultCameraControlSystem.hpp"


// core idea: override this class in whatever application you are making, and create your own scene using entities and components
class GtsScene
{
    protected:
        ECSWorld ecsWorld;
    public:
        virtual ~GtsScene() = default;

        // call this once whenever the scene is loaded
        virtual void onLoad(SceneContext& ctx,
                            const GtsSceneTransitionData* data = nullptr) = 0;

        // Called once per simulation tick at the fixed tick rate.
        // Game logic, AI, movement, and turn processing go here.
        virtual void onUpdateSimulation(SceneContext& ctx) = 0;

        // Called once per rendered frame regardless of tick rate.
        // Input processing, visual updates, and rendering prep go here.
        virtual void onUpdateControllers(SceneContext& ctx) {}

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
            ecsWorld.addControllerSystem<DefaultCameraControlSystem>();
        }
};
