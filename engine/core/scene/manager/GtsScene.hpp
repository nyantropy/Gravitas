#pragma once

#include "ECSWorld.hpp"
#include "EcsSimulationContext.hpp"
#include "EcsControllerContext.hpp"
#include "GtsSceneTransitionData.h"
#include "GtsFrameStats.h"
#include "PhysicsWorld.h"

#include "RenderGpuSystem.hpp"
#include "CameraLifecycleSystem.hpp"
#include "CameraGpuSystem.hpp"
#include "CameraGpuComponent.h"
#include "CameraDescriptionComponent.h"
#include "ActiveCameraViewSystem.hpp"
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
            gts::rendering::resetBindingLifecycleState(ecsWorld);
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
            ecsWorld.registerAddCallback<StaticMeshComponent>(
                [](ECSWorld& world, Entity entity, StaticMeshComponent&)
                {
                    gts::rendering::queueStaticMeshRefresh(world, entity);
                });
            ecsWorld.registerRemoveCallback<StaticMeshComponent>(
                [](ECSWorld& world, Entity entity, StaticMeshComponent&)
                {
                    gts::rendering::queueCleanup(world, entity);
                });
            ecsWorld.registerAddCallback<ProceduralMeshComponent>(
                [](ECSWorld& world, Entity entity, ProceduralMeshComponent&)
                {
                    gts::rendering::queueProceduralRefresh(world, entity);
                });
            ecsWorld.registerRemoveCallback<ProceduralMeshComponent>(
                [](ECSWorld& world, Entity entity, ProceduralMeshComponent&)
                {
                    gts::rendering::queueCleanup(world, entity);
                });
            ecsWorld.registerAddCallback<MaterialComponent>(
                [](ECSWorld& world, Entity entity, MaterialComponent&)
                {
                    gts::rendering::queueStaticMeshRefresh(world, entity);
                    gts::rendering::queueProceduralRefresh(world, entity);
                });
            ecsWorld.registerRemoveCallback<MaterialComponent>(
                [](ECSWorld& world, Entity entity, MaterialComponent&)
                {
                    gts::rendering::queueCleanup(world, entity);
                });
            ecsWorld.registerAddCallback<WorldTextComponent>(
                [](ECSWorld& world, Entity entity, WorldTextComponent&)
                {
                    gts::rendering::queueProceduralRefresh(world, entity);
                });
            ecsWorld.registerRemoveCallback<WorldTextComponent>(
                [](ECSWorld& world, Entity entity, WorldTextComponent&)
                {
                    gts::rendering::queueCleanup(world, entity);
                });
            ecsWorld.registerAddCallback<CameraDescriptionComponent>(
                [](ECSWorld& world, Entity entity, CameraDescriptionComponent&)
                {
                    gts::rendering::queueCameraRefresh(world, entity);
                });
            ecsWorld.registerRemoveCallback<CameraDescriptionComponent>(
                [](ECSWorld& world, Entity entity, CameraDescriptionComponent&)
                {
                    gts::rendering::queueCameraCleanup(world, entity);
                });

            DebugFreeCameraSystem::ensureDebugCameraState(ecsWorld, ctx.windowAspectRatio);

            ecsWorld.addControllerSystem<StaticMeshBindingSystem>();
            ecsWorld.addControllerSystem<ProceduralMeshBindingSystem>();
            ecsWorld.addControllerSystem<WorldTextBindingSystem>();
            ecsWorld.addControllerSystem<RenderGpuSystem>();
            ecsWorld.addControllerSystem<CameraLifecycleSystem>();
            ecsWorld.addControllerSystem<CameraGpuSystem>();
            ecsWorld.addControllerSystem<DebugFreeCameraSystem>();
            ecsWorld.addControllerSystem<CameraBindingSystem>();
            ecsWorld.addControllerSystem<ActiveCameraViewSystem>();
            ecsWorld.addControllerSystem<DefaultCameraControlSystem>();
            ecsWorld.forEachSnapshot<StaticMeshComponent, MaterialComponent>(
                [this](Entity entity, StaticMeshComponent&, MaterialComponent&)
                {
                    // Some legacy scenes still spawn descriptors before installing
                    // the renderer feature. Queue a one-time bootstrap rather than
                    // reintroducing a full steady-state scan.
                    gts::rendering::queueStaticMeshRefresh(ecsWorld, entity);
                });
            ecsWorld.forEachSnapshot<ProceduralMeshComponent, MaterialComponent>(
                [this](Entity entity, ProceduralMeshComponent&, MaterialComponent&)
                {
                    gts::rendering::queueProceduralRefresh(ecsWorld, entity);
                });
            ecsWorld.forEachSnapshot<CameraDescriptionComponent>(
                [this](Entity entity, CameraDescriptionComponent&)
                {
                    gts::rendering::queueCameraRefresh(ecsWorld, entity);
                });
            rendererFeatureInstalled = true;
        }
};
