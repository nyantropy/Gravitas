#pragma once

#include "ECSWorld.hpp"
#include "EcsSimulationContext.hpp"
#include "EcsControllerContext.hpp"
#include "GtsSceneTransitionData.h"
#include "GtsFrameStats.h"
#include "PhysicsWorld.h"

#include "RenderGpuSystem.hpp"
#include "CameraGpuSystem.hpp"
#include "CameraGpuComponent.h"
#include "StaticMeshBindingSystem.hpp"
#include "ProceduralMeshBindingSystem.hpp"
#include "WorldTextBindingSystem.hpp"
#include "RenderBindingLifecycle.h"
#include "CameraBindingSystem.hpp"
#include "MeshGpuComponent.h"
#include "RenderGpuComponent.h"
#include "DebugFreeCameraSystem.hpp"
#include "DefaultCameraControlSystem.hpp"
#include "PhysicsSystem.h"
#include "PhysicsDebugRenderer.h"
#include "RenderExtractionSnapshotBuilder.hpp"
#include "TransformDirtyComponent.h"
#include "TransformDirtyHelpers.h"


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

        // Called once whenever the scene is loaded.
        // ctx is non-const because installPhysicsFeature() writes ctx.physics back
        // to inform the caller (GravitasEngine) of the newly created physics world.
        virtual void onLoad(EcsControllerContext& ctx,
                            const GtsSceneTransitionData* data = nullptr) = 0;

        // Called once per simulation tick at the fixed tick rate.
        // Deterministic scene simulation goes here.
        virtual void onUpdateSimulation(const EcsSimulationContext& ctx) = 0;

        // Called once per rendered frame regardless of tick rate.
        // Input processing, visual updates, and rendering prep go here.
        virtual void onUpdateControllers(const EcsControllerContext& ctx) {}

        virtual void populateFrameStats(GtsFrameStats& /*stats*/) const {}

        ECSWorld& getWorld()
        {
            return ecsWorld;
        }

        IGtsPhysicsModule* getPhysicsModule()
        {
            return physicsWorld.get();
        }

        // Call from onLoad() to enable physics for this scene. Idempotent —
        // safe to call multiple times (e.g., on scene reload); subsequent calls
        // are no-ops so the physics world is not reset or duplicated.
        inline void installPhysicsFeature(EcsControllerContext& ctx, bool enableDebugRenderer = false)
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

        // Call from onLoad() to install the standard rendering systems (mesh binding,
        // GPU sync, camera). Idempotent — safe to call multiple times; subsequent
        // calls are no-ops so systems are not registered twice on scene reload.
        inline void installRendererFeature(const EcsControllerContext& ctx)
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
                [resources](ECSWorld& world, Entity entity, RenderGpuComponent& renderGpu)
                {
                    RenderExtractionSnapshotBuilder::notifyRenderableRemoved(world, entity, renderGpu);

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
            ecsWorld.registerAddCallback<TransformComponent>(
                [](ECSWorld& world, Entity entity, TransformComponent&)
                {
                    gts::transform::markDirty(world, entity);
                });

            ecsWorld.addControllerSystem<WorldTextBindingSystem>();
            ecsWorld.addControllerSystem<RenderGpuSystem>();
            ecsWorld.addControllerSystem<CameraGpuSystem>();
            ecsWorld.addControllerSystem<DebugFreeCameraSystem>();
            ecsWorld.addControllerSystem<CameraBindingSystem>();
            ecsWorld.addControllerSystem<DefaultCameraControlSystem>();

            ecsWorld.registerAddCallback<StaticMeshComponent>(
                [resources](ECSWorld& world, Entity entity, StaticMeshComponent&)
                {
                    gts::rendering::syncStaticMeshBinding(world, entity, resources);
                });
            ecsWorld.registerAddCallback<ProceduralMeshComponent>(
                [resources](ECSWorld& world, Entity entity, ProceduralMeshComponent&)
                {
                    gts::rendering::syncProceduralMeshBinding(world, entity, resources);
                });
            ecsWorld.registerAddCallback<MaterialComponent>(
                [resources](ECSWorld& world, Entity entity, MaterialComponent&)
                {
                    if (world.hasComponent<ProceduralMeshComponent>(entity))
                        gts::rendering::syncProceduralMeshBinding(world, entity, resources);
                    else if (world.hasComponent<StaticMeshComponent>(entity))
                        gts::rendering::syncStaticMeshBinding(world, entity, resources);
                });

            ecsWorld.registerRemoveCallback<StaticMeshComponent>(
                [resources](ECSWorld& world, Entity entity, StaticMeshComponent&)
                {
                    gts::rendering::cleanupRenderableBinding(world, entity, resources);
                });
            ecsWorld.registerRemoveCallback<ProceduralMeshComponent>(
                [resources](ECSWorld& world, Entity entity, ProceduralMeshComponent&)
                {
                    gts::rendering::cleanupRenderableBinding(world, entity, resources);
                });
            ecsWorld.registerRemoveCallback<MaterialComponent>(
                [resources](ECSWorld& world, Entity entity, MaterialComponent&)
                {
                    if (world.hasComponent<StaticMeshComponent>(entity)
                        || world.hasComponent<ProceduralMeshComponent>(entity))
                    {
                        gts::rendering::cleanupRenderableBinding(world, entity, resources);
                    }
                });

            // Many scenes create renderable entities before installRendererFeature().
            // Bootstrap those existing descriptors once at install time; all later
            // changes are handled through add/remove callbacks.
            ecsWorld.forEach<StaticMeshComponent, MaterialComponent>(
                [this, resources](Entity entity, StaticMeshComponent&, MaterialComponent&)
                {
                    gts::rendering::syncStaticMeshBinding(ecsWorld, entity, resources);
                });
            ecsWorld.forEach<ProceduralMeshComponent, MaterialComponent>(
                [this, resources](Entity entity, ProceduralMeshComponent&, MaterialComponent&)
                {
                    gts::rendering::syncProceduralMeshBinding(ecsWorld, entity, resources);
                });
            ecsWorld.forEach<TransformComponent>(
                [this](Entity entity, TransformComponent&)
                {
                    gts::transform::markDirty(ecsWorld, entity);
                });
            rendererFeatureInstalled = true;
        }
};
