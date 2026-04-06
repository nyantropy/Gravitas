#pragma once

#include "ECSWorld.hpp"
#include "SceneContext.h"
#include "GtsSceneTransitionData.h"
#include "GtsFrameStats.h"
#include "PhysicsWorld.h"

#include "RenderGpuSystem.hpp"
#include "CameraGpuSystem.hpp"
#include "CameraGpuComponent.h"
#include "StaticMeshBindingSystem.hpp"
#include "ProceduralMeshBindingSystem.hpp"
#include "WorldTextBindingSystem.hpp"
#include "CameraBindingSystem.hpp"
#include "MeshGpuComponent.h"
#include "RenderGpuComponent.h"
#include "DebugFreeCameraSystem.hpp"
#include "DefaultCameraControlSystem.hpp"
#include "PhysicsSystem.h"
#include "PhysicsDebugRenderer.h"


// Override this class to define a reusable engine scene in terms of entities,
// components, and engine systems.
class GtsScene
{
    protected:
        ECSWorld ecsWorld;
        std::unique_ptr<PhysicsWorld> physicsWorld;
        bool rendererFeatureInstalled = false;
        bool physicsFeatureInstalled  = false;

        void resetSceneWorld()
        {
            ecsWorld.clear();
            physicsWorld.reset();
            rendererFeatureInstalled = false;
            physicsFeatureInstalled  = false;
        }
    public:
        virtual ~GtsScene() = default;

        // call this once whenever the scene is loaded
        virtual void onLoad(SceneContext& ctx,
                            const GtsSceneTransitionData* data = nullptr) = 0;

        // Called once per simulation tick at the fixed tick rate.
        // Deterministic scene simulation goes here.
        virtual void onUpdateSimulation(SceneContext& ctx) = 0;

        // Called once per rendered frame regardless of tick rate.
        // Input processing, visual updates, and rendering prep go here.
        virtual void onUpdateControllers(SceneContext& ctx) {}

        virtual void populateFrameStats(GtsFrameStats& /*stats*/) const {}

        ECSWorld& getWorld()
        {
            return ecsWorld;
        }

        IGtsPhysicsModule* getPhysicsModule()
        {
            return physicsWorld.get();
        }

        inline void installPhysicsFeature(SceneContext& ctx, bool enableDebugRenderer = false)
        {
            if (physicsFeatureInstalled)
                return;

            physicsWorld = std::make_unique<PhysicsWorld>(&ecsWorld);
            ctx.physics  = physicsWorld.get();

            ecsWorld.addSimulationSystem<PhysicsSystem>(physicsWorld.get());
            if (enableDebugRenderer)
                ecsWorld.addControllerSystem<PhysicsDebugRenderer>();
            physicsFeatureInstalled = true;
        }

        // Predefined rendering systems. Call once from onLoad() after scene setup.
        inline void installRendererFeature(SceneContext& ctx)
        {
            if (rendererFeatureInstalled)
                return;

            auto* resources = ctx.resources;
            ecsWorld.registerRemoveCallback<CameraGpuComponent>(
                [resources](ECSWorld&, Entity, CameraGpuComponent& cameraGpu)
                {
                    if (cameraGpu.viewID == 0 || resources == nullptr)
                        return;

                    resources->releaseCameraBuffer(cameraGpu.viewID);
                    cameraGpu.viewID = 0;
                });
            ecsWorld.registerRemoveCallback<RenderGpuComponent>(
                [resources](ECSWorld&, Entity, RenderGpuComponent& renderGpu)
                {
                    if (renderGpu.objectSSBOSlot == RENDERABLE_SLOT_UNALLOCATED
                        || resources == nullptr)
                        return;

                    resources->releaseObjectSlot(renderGpu.objectSSBOSlot);
                    renderGpu.objectSSBOSlot = RENDERABLE_SLOT_UNALLOCATED;
                });
            ecsWorld.registerRemoveCallback<MeshGpuComponent>(
                [resources](ECSWorld&, Entity, MeshGpuComponent& meshGpu)
                {
                    if (!meshGpu.ownsProceduralMeshResource
                        || meshGpu.meshID == 0
                        || resources == nullptr)
                        return;

                    resources->releaseProceduralMesh(meshGpu.meshID);
                    meshGpu.meshID = 0;
                    meshGpu.ownsProceduralMeshResource = false;
                });

            ecsWorld.addControllerSystem<RenderGpuSystem>();
            ecsWorld.addControllerSystem<CameraGpuSystem>();
            ecsWorld.addControllerSystem<StaticMeshBindingSystem>();
            ecsWorld.addControllerSystem<ProceduralMeshBindingSystem>();
            ecsWorld.addControllerSystem<WorldTextBindingSystem>();
            ecsWorld.addControllerSystem<DebugFreeCameraSystem>();
            ecsWorld.addControllerSystem<CameraBindingSystem>();
            ecsWorld.addControllerSystem<DefaultCameraControlSystem>();
            rendererFeatureInstalled = true;
        }
};
