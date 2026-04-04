#pragma once

#include "ECSWorld.hpp"
#include "SceneContext.h"
#include "GtsSceneTransitionData.h"
#include "PhysicsWorld.h"

#include "RenderGpuSystem.hpp"
#include "CameraGpuSystem.hpp"
#include "StaticMeshBindingSystem.hpp"
#include "ProceduralMeshBindingSystem.hpp"
#include "WorldTextBindingSystem.hpp"
#include "CameraBindingSystem.hpp"
#include "RenderResourceClearSystem.hpp"
#include "CameraResourceClearSystem.hpp"
#include "DebugFreeCameraSystem.hpp"
#include "DefaultCameraControlSystem.hpp"
#include "PhysicsSystem.h"
#include "PhysicsDebugRenderer.h"


// core idea: override this class in whatever application you are making, and create your own scene using entities and components
class GtsScene
{
    protected:
        ECSWorld ecsWorld;
        std::unique_ptr<PhysicsWorld> physicsWorld;
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

        IGtsPhysicsModule* getPhysicsModule()
        {
            return physicsWorld.get();
        }

        inline void installPhysicsFeature(SceneContext& ctx, bool enableDebugRenderer = true)
        {
            physicsWorld = std::make_unique<PhysicsWorld>(&ecsWorld);
            ctx.physics  = physicsWorld.get();

            ecsWorld.addSimulationSystem<PhysicsSystem>(physicsWorld.get());
            if (enableDebugRenderer)
                ecsWorld.addControllerSystem<PhysicsDebugRenderer>();
        }

        // pre defined rendering systems - should be called once in the onLoad() function of whatever scene you design
        inline void installRendererFeature()
        {
            ecsWorld.addControllerSystem<RenderGpuSystem>();
            ecsWorld.addControllerSystem<CameraGpuSystem>();
            ecsWorld.addControllerSystem<StaticMeshBindingSystem>();
            ecsWorld.addControllerSystem<ProceduralMeshBindingSystem>();
            ecsWorld.addControllerSystem<WorldTextBindingSystem>();
            ecsWorld.addControllerSystem<DebugFreeCameraSystem>();
            ecsWorld.addControllerSystem<CameraBindingSystem>();
            ecsWorld.addControllerSystem<RenderResourceClearSystem>();
            ecsWorld.addControllerSystem<CameraResourceClearSystem>();
            ecsWorld.addControllerSystem<DefaultCameraControlSystem>();
        }
};
