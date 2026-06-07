#pragma once

#include "ECSWorld.hpp"
#include "EcsSimulationContext.hpp"
#include "EcsControllerContext.hpp"
#include "GtsSceneTransitionData.h"
#include "GtsFrameStats.h"
#include "PhysicsWorld.h"

#include "RenderGpuSystem.hpp"
#include "TextureAnimationSystem.hpp"
#include "CameraLifecycleSystem.hpp"
#include "CameraGpuSystem.hpp"
#include "CameraGpuComponent.h"
#include "CameraDescriptionComponent.h"
#include "ActiveCameraViewSystem.hpp"
#include "RenderableCleanupSystem.hpp"
#include "StaticMeshBindingSystem.hpp"
#include "QuadMeshBindingSystem.hpp"
#include "DynamicMeshBindingSystem.hpp"
#include "WorldTextBindingSystem.hpp"
#include "GeometryBindingLifecycle.h"
#include "CameraBindingLifecycle.h"
#include "CameraBindingSystem.hpp"
#include "MeshGpuComponent.h"
#include "RenderDirtyComponent.h"
#include "RenderGpuComponent.h"
#include "DebugFreeCameraSystem.hpp"
#include "DefaultCameraControlSystem.hpp"
#include "PhysicsSystem.h"
#include "PhysicsDebugRenderer.h"
#include "ParticleEffectHotReloadSystem.hpp"
#include "ParticleEmitterSystem.hpp"
#include "TextureAnimationComponent.h"
#include "TextureAnimationRuntimeComponent.h"
#include "WorldTextRuntimeComponent.h"
#include "DebugDrawSystem.hpp"
#include "RenderExtractionSnapshotBuilder.hpp"
#include "TransformDirtyHelpers.h"
#include "InputBindingRegistry.h"
#include "QuadMeshComponent.h"
#include "DynamicMeshComponent.h"

// Override this class to define a reusable engine scene in terms of entities,
// components, and engine systems.
class GtsScene
{
    protected:
    ECSWorld                      ecsWorld;
    std::unique_ptr<PhysicsWorld> physicsWorld;
    bool                          rendererFeatureInstalled  = false;
    bool                          physicsFeatureInstalled   = false;
    bool                          debugDrawFeatureInstalled = false;

    void resetSceneWorld()
    {
        gts::rendering::resetGeometryBindingLifecycleState(ecsWorld);
        gts::rendering::resetCameraBindingLifecycleState(ecsWorld);
        ecsWorld.clear();
        physicsWorld.reset();
        rendererFeatureInstalled  = false;
        physicsFeatureInstalled   = false;
        debugDrawFeatureInstalled = false;
    }

    public:
    virtual ~GtsScene() = default;

    // Called once whenever the scene is loaded.
    // ctx is non-const because installPhysicsFeature() writes ctx.physics back
    // to inform the caller (GravitasEngine) of the newly created physics world.
    virtual void onLoad(EcsControllerContext& ctx, const GtsSceneTransitionData* data = nullptr) = 0;

    // Called once per simulation tick at the fixed tick rate.
    // Deterministic scene simulation goes here.
    virtual void onUpdateSimulation(const EcsSimulationContext& ctx) = 0;

    // Called once per rendered frame regardless of tick rate.
    // Input processing, visual updates, and rendering prep go here.
    virtual void onUpdateControllers(const EcsControllerContext& ctx) {}

    virtual void onUnload(EcsControllerContext& /*ctx*/) {}

    void unload(EcsControllerContext& ctx)
    {
        onUnload(ctx);
        resetSceneWorld();
    }

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

                if (renderGpu.objectSSBOSlot == RENDERABLE_SLOT_UNALLOCATED || resources == nullptr)
                    return;

                resources->releaseObjectSlot(renderGpu.objectSSBOSlot);
                renderGpu.objectSSBOSlot = RENDERABLE_SLOT_UNALLOCATED;
            });
        ecsWorld.registerRemoveCallback<MeshGpuComponent>(
            [resources](ECSWorld&, Entity, MeshGpuComponent& meshGpu)
            {
                if (!meshGpu.ownsProceduralMeshResource || meshGpu.meshID == 0 || resources == nullptr)
                    return;

                resources->releaseProceduralMesh(meshGpu.meshID);
                meshGpu.meshID                     = 0;
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
                gts::rendering::queueRenderableCleanup(world, entity);
            });
        ecsWorld.registerAddCallback<QuadMeshComponent>(
            [](ECSWorld& world, Entity entity, QuadMeshComponent&)
            {
                gts::rendering::queueQuadMeshRefresh(world, entity);
            });
        ecsWorld.registerRemoveCallback<QuadMeshComponent>(
            [](ECSWorld& world, Entity entity, QuadMeshComponent&)
            {
                gts::rendering::queueRenderableCleanup(world, entity);
            });
        ecsWorld.registerAddCallback<DynamicMeshComponent>(
            [](ECSWorld& world, Entity entity, DynamicMeshComponent&)
            {
                gts::rendering::queueDynamicMeshRefresh(world, entity);
            });
        ecsWorld.registerRemoveCallback<DynamicMeshComponent>(
            [](ECSWorld& world, Entity entity, DynamicMeshComponent&)
            {
                gts::rendering::queueRenderableCleanup(world, entity);
            });
        ecsWorld.registerAddCallback<MaterialComponent>(
            [](ECSWorld& world, Entity entity, MaterialComponent&)
            {
                gts::rendering::queueStaticMeshRefresh(world, entity);
                gts::rendering::queueQuadMeshRefresh(world, entity);
                gts::rendering::queueDynamicMeshRefresh(world, entity);
            });
        ecsWorld.registerRemoveCallback<MaterialComponent>(
            [](ECSWorld& world, Entity entity, MaterialComponent&)
            {
                gts::rendering::queueRenderableCleanup(world, entity);
            });
        ecsWorld.registerAddCallback<TextureAnimationComponent>(
            [](ECSWorld& world, Entity entity, TextureAnimationComponent&)
            {
                if (!world.hasComponent<TextureAnimationRuntimeComponent>(entity))
                    world.commands().addComponent<TextureAnimationRuntimeComponent>(entity,
                                                                                    TextureAnimationRuntimeComponent{});

                if (!world.hasComponent<RenderGpuComponent>(entity) ||
                    !world.hasComponent<RenderDirtyComponent>(entity))
                {
                    return;
                }

                auto& renderGpu       = world.getComponent<RenderGpuComponent>(entity);
                auto& dirty           = world.getComponent<RenderDirtyComponent>(entity);
                renderGpu.uvTransform = {1.0f, 1.0f, 0.0f, 0.0f};
                dirty.objectDataDirty = true;
                gts::rendering::queueRenderSnapshotDirty(world, entity);
            });
        ecsWorld.registerRemoveCallback<TextureAnimationComponent>(
            [](ECSWorld& world, Entity entity, TextureAnimationComponent&)
            {
                if (world.hasComponent<TextureAnimationRuntimeComponent>(entity))
                    world.commands().removeComponent<TextureAnimationRuntimeComponent>(entity);

                if (!world.hasComponent<RenderGpuComponent>(entity) ||
                    !world.hasComponent<RenderDirtyComponent>(entity))
                {
                    return;
                }

                auto& renderGpu       = world.getComponent<RenderGpuComponent>(entity);
                auto& dirty           = world.getComponent<RenderDirtyComponent>(entity);
                renderGpu.uvTransform = {1.0f, 1.0f, 0.0f, 0.0f};
                dirty.objectDataDirty = true;
                gts::rendering::queueRenderSnapshotDirty(world, entity);
            });
        ecsWorld.registerAddCallback<WorldTextComponent>(
            [](ECSWorld& world, Entity entity, WorldTextComponent&)
            {
                if (!world.hasComponent<WorldTextRuntimeComponent>(entity))
                    world.commands().addComponent<WorldTextRuntimeComponent>(entity, WorldTextRuntimeComponent{});
            });
        ecsWorld.registerRemoveCallback<WorldTextComponent>(
            [](ECSWorld& world, Entity entity, WorldTextComponent&)
            {
                if (world.hasComponent<WorldTextRuntimeComponent>(entity))
                    world.commands().removeComponent<WorldTextRuntimeComponent>(entity);

                gts::rendering::queueRenderableCleanup(world, entity);
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

        DebugFreeCameraSystem::ensureDebugCameraState(ecsWorld,
                                                      ctx.sceneViewportAspectRatio > 0.0f
                                                          ? ctx.sceneViewportAspectRatio
                                                          : ctx.windowAspectRatio);
        if (ctx.input != nullptr)
            ctx.input->popContext(DebugFreeCameraSystem::InputContext);

        ecsWorld.addControllerSystem<StaticMeshBindingSystem>();
        ecsWorld.addControllerSystem<QuadMeshBindingSystem>();
        ecsWorld.addControllerSystem<DynamicMeshBindingSystem>();
        ecsWorld.addControllerSystem<WorldTextBindingSystem>();
        ecsWorld.addControllerSystem<RenderableCleanupSystem>();
        ecsWorld.addControllerSystem<RenderGpuSystem>();
        ecsWorld.addControllerSystem<TextureAnimationSystem>();
        ecsWorld.addControllerSystem<CameraLifecycleSystem>();
        ecsWorld.addControllerSystem<DefaultCameraControlSystem>();
        ecsWorld.addControllerSystem<CameraGpuSystem>();
        ecsWorld.addControllerSystem<DebugFreeCameraSystem>();
        ecsWorld.addControllerSystem<CameraBindingSystem>();
        ecsWorld.addControllerSystem<ActiveCameraViewSystem>();
        ecsWorld.addControllerSystem<ParticleEffectHotReloadSystem>();
        ecsWorld.addControllerSystem<ParticleEmitterSystem>();
        ecsWorld.forEachSnapshot<StaticMeshComponent, MaterialComponent>(
            [this](Entity entity, StaticMeshComponent&, MaterialComponent&)
            {
                // Some legacy scenes still spawn descriptors before installing
                // the renderer feature. Queue a one-time bootstrap rather than
                // reintroducing a full steady-state scan.
                gts::rendering::queueStaticMeshRefresh(ecsWorld, entity);
            });
        ecsWorld.forEachSnapshot<QuadMeshComponent, MaterialComponent>(
            [this](Entity entity, QuadMeshComponent&, MaterialComponent&)
            {
                gts::rendering::queueQuadMeshRefresh(ecsWorld, entity);
            });
        ecsWorld.forEachSnapshot<DynamicMeshComponent, MaterialComponent>(
            [this](Entity entity, DynamicMeshComponent&, MaterialComponent&)
            {
                gts::rendering::queueDynamicMeshRefresh(ecsWorld, entity);
            });
        ecsWorld.forEachSnapshot<TextureAnimationComponent>(
            [this](Entity entity, TextureAnimationComponent&)
            {
                if (!ecsWorld.hasComponent<TextureAnimationRuntimeComponent>(entity))
                    ecsWorld.commands().addComponent<TextureAnimationRuntimeComponent>(
                        entity, TextureAnimationRuntimeComponent{});
            });
        ecsWorld.forEachSnapshot<WorldTextComponent>(
            [this](Entity entity, WorldTextComponent&)
            {
                if (!ecsWorld.hasComponent<WorldTextRuntimeComponent>(entity))
                    ecsWorld.commands().addComponent<WorldTextRuntimeComponent>(entity, WorldTextRuntimeComponent{});
            });
        ecsWorld.forEachSnapshot<CameraDescriptionComponent>(
            [this](Entity entity, CameraDescriptionComponent&)
            {
                gts::rendering::queueCameraRefresh(ecsWorld, entity);
            });
        rendererFeatureInstalled = true;
    }

    inline void installDebugDrawFeature(const EcsControllerContext&)
    {
        if (debugDrawFeatureInstalled)
            return;

        ecsWorld.addControllerSystem<gts::debugdraw::DebugDrawSystem>();
        debugDrawFeatureInstalled = true;
    }
};
