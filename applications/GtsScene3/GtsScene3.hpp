#pragma once

#include <chrono>
#include <cstdio>
#include <cstdint>
#include <random>
#include <string>
#include <vector>

#include "GtsScene.hpp"

#include "BoundsComponent.h"
#include "CameraBindingSystem.hpp"
#include "CameraDescriptionComponent.h"
#include "CameraGpuComponent.h"
#include "CameraGpuSystem.hpp"
#include "CubeAnimationComponent.h"
#include "CubeAnimationSystem.hpp"
#include "DebugFreeCameraSystem.hpp"
#include "DefaultCameraControlSystem.hpp"
#include "EngineConfig.h"
#include "Entity.h"
#include "GraphicsConstants.h"
#include "MaterialComponent.h"
#include "MeshGpuComponent.h"
#include "RenderBindingLifecycle.h"
#include "RenderExtractionSnapshotBuilder.hpp"
#include "RenderGpuComponent.h"
#include "RenderGpuSystem.hpp"
#include "StaticMeshBindingSystem.hpp"
#include "StaticMeshComponent.h"
#include "TransformComponent.h"
#include "TransformDirtyHelpers.h"

class GtsScene3 : public GtsScene
{
public:
    GtsScene3() = default;

    void onLoad(EcsControllerContext& ctx,
                const GtsSceneTransitionData* data = nullptr) override;
    void onUpdateSimulation(const EcsSimulationContext& ctx) override;
    void onUpdateControllers(const EcsControllerContext& ctx) override;
    void populateFrameStats(GtsFrameStats& stats) const override;

private:
    static constexpr uint32_t CubeCount = 15000;
    static constexpr uint32_t GridColumns = 25;
    static constexpr uint32_t GridRows = 24;
    static constexpr uint32_t GridLayers = 25;
    static constexpr float GridSpacing = 2.5f;
    static constexpr float CubeScale = 1.0f;
    static constexpr uint32_t RandomSeed = 1337u;

    std::vector<std::string> texturePaths;

    void buildTextureSet();
    void installStressRendererFeature(const EcsControllerContext& ctx);
    void spawnStressCubes();
    glm::vec3 computeCubePosition(uint32_t index) const;
    void spawnCamera(float aspectRatio);
};

inline void GtsScene3::onLoad(EcsControllerContext& ctx, const GtsSceneTransitionData*)
{
    resetSceneWorld();
    buildTextureSet();
    ecsWorld.addSimulationSystem<CubeAnimationSystem>();
    installStressRendererFeature(ctx);

    const auto loadStart = std::chrono::steady_clock::now();
    spawnStressCubes();
    spawnCamera(ctx.windowAspectRatio);
    const auto loadEnd = std::chrono::steady_clock::now();

    const float loadMs = std::chrono::duration<float, std::milli>(loadEnd - loadStart).count();
    std::printf("GtsScene3: spawned %u cubes in %.2f ms\n", CubeCount, loadMs);
}

inline void GtsScene3::onUpdateSimulation(const EcsSimulationContext& ctx)
{
    ecsWorld.updateSimulation(ctx);
}

inline void GtsScene3::onUpdateControllers(const EcsControllerContext& ctx)
{
    ecsWorld.updateControllers(ctx);
}

inline void GtsScene3::populateFrameStats(GtsFrameStats& stats) const
{
    stats.sceneEntityCount = CubeCount;
}

inline void GtsScene3::buildTextureSet()
{
    texturePaths = {
        GraphicsConstants::ENGINE_RESOURCES + "/textures/blue_texture.png",
        GraphicsConstants::ENGINE_RESOURCES + "/textures/cyan_texture.png",
        GraphicsConstants::ENGINE_RESOURCES + "/textures/green_texture.png",
        GraphicsConstants::ENGINE_RESOURCES + "/textures/grey_texture.png",
        GraphicsConstants::ENGINE_RESOURCES + "/textures/orange_texture.png",
        GraphicsConstants::ENGINE_RESOURCES + "/textures/purple_texture.png",
        GraphicsConstants::ENGINE_RESOURCES + "/textures/red_texture.png",
        GraphicsConstants::ENGINE_RESOURCES + "/textures/yellow_texture.png"
    };
}

inline void GtsScene3::installStressRendererFeature(const EcsControllerContext& ctx)
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
    ecsWorld.registerAddCallback<MaterialComponent>(
        [](ECSWorld& world, Entity entity, MaterialComponent&)
        {
            gts::rendering::queueStaticMeshRefresh(world, entity);
        });
    ecsWorld.registerRemoveCallback<MaterialComponent>(
        [](ECSWorld& world, Entity entity, MaterialComponent&)
        {
            gts::rendering::queueCleanup(world, entity);
        });

    DebugFreeCameraSystem::ensureDebugCameraState(ecsWorld, ctx.windowAspectRatio);

    ecsWorld.addControllerSystem<StaticMeshBindingSystem>();
    ecsWorld.addControllerSystem<RenderGpuSystem>();
    ecsWorld.addControllerSystem<CameraGpuSystem>();
    ecsWorld.addControllerSystem<DebugFreeCameraSystem>();
    ecsWorld.addControllerSystem<CameraBindingSystem>();
    ecsWorld.addControllerSystem<DefaultCameraControlSystem>();

    rendererFeatureInstalled = true;
}

inline void GtsScene3::spawnStressCubes()
{
    if (texturePaths.empty())
        return;

    static_assert(GridColumns * GridRows * GridLayers == CubeCount,
                  "CubeCount must match the 3D stress grid dimensions");
    static_assert(CubeCount < EngineConfig::MAX_RENDERABLE_OBJECTS,
                  "CubeCount exceeds the renderer object budget");

    constexpr float pi = 3.14159265358979323846f;
    const std::string cubeMesh = GraphicsConstants::ENGINE_RESOURCES + "/models/cube.obj";

    std::mt19937 rng(RandomSeed);
    std::uniform_int_distribution<int> animationTypeDist(0, 3);
    std::uniform_int_distribution<size_t> textureDist(0, texturePaths.size() - 1);
    std::uniform_real_distribution<float> phaseDist(0.0f, pi * 2.0f);
    std::uniform_real_distribution<float> speedDist(0.7f, 1.8f);
    std::uniform_real_distribution<float> amplitudeDist(0.35f, 1.05f);
    std::uniform_real_distribution<float> orbitRadiusDist(0.5f, 1.5f);
    std::uniform_real_distribution<float> rotationAmountDist(0.4f, 1.35f);
    std::uniform_real_distribution<float> axisDist(-1.0f, 1.0f);

    for (uint32_t index = 0; index < CubeCount; ++index)
    {
        Entity entity = ecsWorld.createEntity();
        const glm::vec3 homePosition = computeCubePosition(index);

        TransformComponent transform;
        transform.position = homePosition;
        transform.scale = glm::vec3(CubeScale);
        ecsWorld.addComponent(entity, transform);

        StaticMeshComponent mesh;
        mesh.meshPath = cubeMesh;
        ecsWorld.addComponent(entity, mesh);

        MaterialComponent material;
        material.texturePath = texturePaths[textureDist(rng)];
        ecsWorld.addComponent(entity, material);

        CubeAnimationComponent animation;
        animation.animationType = animationTypeDist(rng);
        animation.homePosition = homePosition;
        animation.phase = phaseDist(rng);
        animation.speed = speedDist(rng);
        animation.axis = glm::vec3(axisDist(rng), axisDist(rng), axisDist(rng));
        if (glm::dot(animation.axis, animation.axis) <= 0.0001f)
            animation.axis = glm::vec3(0.0f, 1.0f, 0.0f);
        else
            animation.axis = glm::normalize(animation.axis);
        animation.amplitude = amplitudeDist(rng);
        animation.orbitRadius = orbitRadiusDist(rng);
        animation.rotationAmount = rotationAmountDist(rng);
        ecsWorld.addComponent(entity, animation);

        BoundsComponent bounds;
        bounds.min = glm::vec3(-0.5f, -0.5f, -0.5f);
        bounds.max = glm::vec3(0.5f, 0.5f, 0.5f);
        ecsWorld.addComponent(entity, bounds);
    }
}

inline glm::vec3 GtsScene3::computeCubePosition(uint32_t index) const
{
    const uint32_t x = index % GridColumns;
    const uint32_t y = (index / GridColumns) % GridRows;
    const uint32_t z = index / (GridColumns * GridRows);

    const float halfColumns = (static_cast<float>(GridColumns) - 1.0f) * 0.5f;
    const float halfRows = (static_cast<float>(GridRows) - 1.0f) * 0.5f;
    const float halfLayers = (static_cast<float>(GridLayers) - 1.0f) * 0.5f;

    return {
        (static_cast<float>(x) - halfColumns) * GridSpacing,
        (static_cast<float>(y) - halfRows) * GridSpacing,
        (static_cast<float>(z) - halfLayers) * GridSpacing
    };
}

inline void GtsScene3::spawnCamera(float aspectRatio)
{
    Entity camera = ecsWorld.createEntity();

    CameraDescriptionComponent description;
    description.active = true;
    description.fov = glm::radians(70.0f);
    description.aspectRatio = aspectRatio;
    description.nearClip = 0.1f;
    description.farClip = 500.0f;
    description.target = glm::vec3(0.0f, 0.0f, 0.0f);
    ecsWorld.addComponent(camera, description);
    ecsWorld.addComponent(camera, CameraGpuComponent{});

    TransformComponent transform;
    transform.position = glm::vec3(0.0f, 35.0f, 120.0f);
    ecsWorld.addComponent(camera, transform);
}
