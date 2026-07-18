#pragma once

#include <algorithm>
#include <cmath>
#include <string>
#include <unordered_map>
#include <vector>

#include "CameraDescriptionComponent.h"
#include "CameraBindingLifecycle.h"
#include "CameraBindingSystem.hpp"
#include "CameraGpuComponent.h"
#include "CameraGpuSystem.hpp"
#include "CameraLifecycleSystem.hpp"
#include "ActiveCameraViewSystem.hpp"
#include "DynamicMeshComponent.h"
#include "ECSWorld.hpp"
#include "EcsControllerContext.hpp"
#include "EditorPreviewRenderData.h"
#include "EngineToolViewportCameraControl.h"
#include "FrustumCullingStrategy.h"
#include "GlmConfig.h"
#include "IResourceProvider.hpp"
#include "MaterialReferenceHelpers.h"
#include "ParticleEffectAsset.h"
#include "ParticleEmitterComponent.h"
#include "ParticleEmitterRuntimeComponent.h"
#include "ParticleFrameData.h"
#include "ParticleProgramCompiler.h"
#include "RenderPipeline.h"
#include "RendererGeometrySceneFeature.h"
#include "RendererParticleSceneFeature.h"
#include "TimeContext.h"
#include "TransformComponent.h"
#include "TransformDirtyHelpers.h"
#include "TransformSceneFeature.h"
#include "Vertex.h"

namespace gts::tools
{
    class ParticlePreviewWorld
    {
    public:
        ParticlePreviewWorld()
            : renderPipeline(std::make_unique<FrustumCullingStrategy>(true))
        {
        }

        ~ParticlePreviewWorld()
        {
            abandon();
        }

        void ensure(IResourceProvider* resources)
        {
            if (installed || resources == nullptr)
                return;

            resourceProvider = resources;
            gts::transform::installTransformFeature(world);
            gts::rendering::installRendererGeometrySceneFeature(world, resources);
            installPreviewCameraFeature(resources);
            gts::rendering::installRendererParticleSceneFeature(world);
            installed = true;

            createCamera();
            createGrid();
        }

        void destroy()
        {
            emitterEntities.clear();
            cameraEntity = INVALID_ENTITY;
            gridEntity = INVALID_ENTITY;
            currentPath.clear();
            cameraState = {};
            world.clear();
            renderPipeline.resetSceneState();
            installed = false;
            resourceProvider = nullptr;
        }

        void syncAsset(const std::string& path,
                       const ParticleEffectAsset& asset,
                       float playbackTimeScale,
                       bool playbackPaused)
        {
            if (path != currentPath)
                cameraState = {};
            currentPath = path;
            removeMissingEmitters(asset);

            for (const ParticleEffectEmitter& emitter : asset.emitters)
            {
                if (!emitter.descriptor.enabled || !emitter.compiledProgram.valid)
                {
                    destroyEmitter(emitter.stableId);
                    continue;
                }

                ParticleEmitterComponent descriptor =
                    gts::particles::compiledParticleRuntimeDescriptor(emitter);
                descriptor.effectPath = path;
                descriptor.effectEmitterId = emitter.stableId;
                descriptor.reloadFromEffect = false;
                if (descriptor.randomSeed == 0u)
                    descriptor.randomSeed = 1u;

                Entity entity = entityForEmitter(emitter.stableId);
                if (!world.hasComponent<ParticleEmitterComponent>(entity))
                    world.addComponent(entity, descriptor);
                else
                    world.getComponent<ParticleEmitterComponent>(entity) = descriptor;

                TransformComponent& transform = ensureTransform(entity);
                transform.position = {0.0f, 0.0f, 0.0f};
                transform.rotation = {0.0f, 0.0f, 0.0f};
                transform.scale = {1.0f, 1.0f, 1.0f};
                gts::transform::markDirty(world, entity);

                ParticleEmitterRuntimeComponent& runtime = ensureRuntime(entity, descriptor);
                runtime.playbackTimeScale = playbackTimeScale;
                runtime.playbackPaused = playbackPaused;
                if (runtime.particles.capacity() < descriptor.maxParticles)
                    runtime.particles.reserve(std::max(1u, descriptor.maxParticles));
            }
        }

        void resetSimulation()
        {
            world.forEach<ParticleEmitterRuntimeComponent>(
                [](Entity, ParticleEmitterRuntimeComponent& runtime)
                {
                    runtime.particles.clear();
                    runtime.spawnAccumulator = 0.0f;
                    runtime.emitterAge = 0.0f;
                    runtime.burstRepeatCounts.clear();
                    runtime.textureID = 0;
                    runtime.meshID = 0;
                    runtime.boundTexturePath.clear();
                    runtime.boundMeshPath.clear();
                });
        }

        EditorPreviewRenderData buildFrame(const ParticleEffectAsset& asset,
                                           float dt,
                                           bool playing,
                                           float playbackTimeScale,
                                           uint32_t width,
                                           uint32_t height,
                                           const EngineToolInputCaptureComponent* inputCapture = nullptr)
        {
            EditorPreviewRenderData data;
            if (!installed || resourceProvider == nullptr)
                return data;

            updateCamera(asset, width, height, inputCapture);
            setRuntimePlayback(playbackTimeScale, !playing);

            TimeContext previewTime;
            previewTime.unscaledDeltaTime = std::clamp(dt, 0.0f, 0.1f);
            previewTime.deltaTime = playing ? previewTime.unscaledDeltaTime * playbackTimeScale : 0.0f;
            previewTime.timeScale = playbackTimeScale;
            previewTime.frame = ++frame;

            EcsControllerContext previewCtx{world};
            previewCtx.resources = resourceProvider;
            previewCtx.time = &previewTime;
            previewCtx.windowPixelWidth = static_cast<float>(std::max(1u, width));
            previewCtx.windowPixelHeight = static_cast<float>(std::max(1u, height));
            previewCtx.windowAspectRatio =
                static_cast<float>(std::max(1u, width)) / static_cast<float>(std::max(1u, height));
            previewCtx.sceneViewportPixelWidth = previewCtx.windowPixelWidth;
            previewCtx.sceneViewportPixelHeight = previewCtx.windowPixelHeight;
            previewCtx.sceneViewportAspectRatio = previewCtx.windowAspectRatio;

            world.updateControllers(previewCtx);

            const std::vector<RenderCommand>& commands = renderPipeline.build(world);
            data.enabled = true;
            data.width = std::max(1u, width);
            data.height = std::max(1u, height);
            data.viewport = RenderViewportRect::full(static_cast<int>(data.width), static_cast<int>(data.height));
            data.renderList = commands;
            data.materialFrameData = renderPipeline.getLatestSnapshot().materialFrameData;
            data.objectUploads = renderPipeline.getLatestSnapshot().objectUploads;
            data.cameraUploads = renderPipeline.getLatestSnapshot().cameraUploads;
            if (world.hasAny<ParticleFrameDataComponent>())
                data.particleData = world.getSingleton<ParticleFrameDataComponent>().frameData;
            return data;
        }

        ECSWorld& ecsWorld()
        {
            return world;
        }

        const ECSWorld& ecsWorld() const
        {
            return world;
        }

    private:
        ECSWorld world;
        RenderPipeline renderPipeline;
        IResourceProvider* resourceProvider = nullptr;
        Entity cameraEntity = INVALID_ENTITY;
        Entity gridEntity = INVALID_ENTITY;
        std::unordered_map<std::string, Entity> emitterEntities;
        std::string currentPath;
        EngineToolOrbitCameraState cameraState;
        bool installed = false;
        uint64_t frame = 0;

        void abandon()
        {
            emitterEntities.clear();
            cameraEntity = INVALID_ENTITY;
            gridEntity = INVALID_ENTITY;
            currentPath.clear();
            cameraState = {};
            installed = false;
            resourceProvider = nullptr;
        }

        void installPreviewCameraFeature(IResourceProvider* resources)
        {
            world.registerRemoveCallback<CameraGpuComponent>(
                [resources](ECSWorld&, Entity, CameraGpuComponent& cameraGpu)
                {
                    if (cameraGpu.viewID == 0 || resources == nullptr)
                        return;

                    resources->releaseCameraBuffer(cameraGpu.viewID);
                    cameraGpu.viewID = 0;
                });
            world.registerAddCallback<CameraDescriptionComponent>(
                [](ECSWorld& world, Entity entity, CameraDescriptionComponent&)
                {
                    gts::rendering::queueCameraRefresh(world, entity);
                });
            world.registerRemoveCallback<CameraDescriptionComponent>(
                [](ECSWorld& world, Entity entity, CameraDescriptionComponent&)
                {
                    gts::rendering::queueCameraCleanup(world, entity);
                });

            world.addControllerSystem<CameraLifecycleSystem>(EcsSystemGroup::Camera);
            world.addControllerSystem<CameraGpuSystem>(EcsSystemGroup::Camera);
            world.addControllerSystem<CameraBindingSystem>(EcsSystemGroup::Camera);
            world.addControllerSystem<ActiveCameraViewSystem>(EcsSystemGroup::Camera);
        }

        void createCamera()
        {
            cameraEntity = world.createEntity();

            TransformComponent transform;
            transform.position = {0.0f, 1.0f, 4.0f};

            CameraDescriptionComponent camera;
            camera.target = {0.0f, 0.6f, 0.0f};
            camera.up = {0.0f, 1.0f, 0.0f};
            camera.fov = glm::radians(70.0f);
            camera.nearClip = 0.05f;
            camera.farClip = 250.0f;
            camera.aspectRatio = 1.0f;
            camera.active = true;

            world.addComponent(cameraEntity, transform);
            world.addComponent(cameraEntity, camera);
        }

        static void addLineRect(std::vector<Vertex>& vertices,
                                std::vector<uint32_t>& indices,
                                glm::vec3 a,
                                glm::vec3 b,
                                float thickness,
                                glm::vec3 color)
        {
            glm::vec3 direction = b - a;
            if (glm::dot(direction, direction) <= 0.000001f)
                return;
            direction = glm::normalize(direction);
            glm::vec3 normal = glm::normalize(glm::cross(direction, glm::vec3(0.0f, 1.0f, 0.0f)));
            normal *= thickness * 0.5f;

            const uint32_t base = static_cast<uint32_t>(vertices.size());
            vertices.push_back({a - normal, color, {0.0f, 0.0f}});
            vertices.push_back({a + normal, color, {1.0f, 0.0f}});
            vertices.push_back({b - normal, color, {0.0f, 1.0f}});
            vertices.push_back({b + normal, color, {1.0f, 1.0f}});
            indices.insert(indices.end(), {base, base + 2, base + 1, base + 1, base + 2, base + 3});
        }

        static void addFloorQuad(std::vector<Vertex>& vertices,
                                 std::vector<uint32_t>& indices,
                                 float extent,
                                 float y,
                                 glm::vec3 color)
        {
            const uint32_t base = static_cast<uint32_t>(vertices.size());
            vertices.push_back({{-extent, y, -extent}, color, {0.0f, 0.0f}});
            vertices.push_back({{ extent, y, -extent}, color, {1.0f, 0.0f}});
            vertices.push_back({{-extent, y,  extent}, color, {0.0f, 1.0f}});
            vertices.push_back({{ extent, y,  extent}, color, {1.0f, 1.0f}});
            indices.insert(indices.end(), {base, base + 2, base + 1, base + 1, base + 2, base + 3});
        }

        void createGrid()
        {
            gridEntity = world.createEntity();

            DynamicMeshComponent mesh;
            const int halfCells = 6;
            const float extent = static_cast<float>(halfCells);
            const float thickness = 0.0120f;
            addFloorQuad(mesh.vertices, mesh.indices, 2.85f, -0.035f, {0.020f, 0.027f, 0.036f});
            for (int i = -halfCells; i <= halfCells; ++i)
            {
                const float p = static_cast<float>(i);
                const glm::vec3 color = i == 0 ? glm::vec3(0.240f, 0.340f, 0.410f)
                                               : glm::vec3(0.075f, 0.098f, 0.125f);
                addLineRect(mesh.vertices, mesh.indices, {-extent, 0.0f, p}, {extent, 0.0f, p}, thickness, color);
                addLineRect(mesh.vertices, mesh.indices, {p, 0.0f, -extent}, {p, 0.0f, extent}, thickness, color);
            }
            addLineRect(mesh.vertices, mesh.indices, {-0.8f, 0.012f, 0.0f}, {0.8f, 0.012f, 0.0f}, 0.030f, {0.180f, 0.330f, 0.620f});
            addLineRect(mesh.vertices, mesh.indices, {0.0f, 0.016f, -0.8f}, {0.0f, 0.016f, 0.8f}, 0.030f, {0.200f, 0.470f, 0.370f});

            gts::rendering::UnlitMaterialDescriptor material;
            material.baseColor = {1.0f, 1.0f, 1.0f, 0.72f};
            material.vertexColorOnly = true;
            material.doubleSided = true;
            material.depthWrite = true;

            TransformComponent transform;
            world.addComponent(gridEntity, transform);
            world.addComponent(gridEntity, mesh);
            world.addComponent(gridEntity, gts::rendering::sharedUnlitMaterialReference(world, material));
        }

        Entity entityForEmitter(const std::string& stableId)
        {
            auto it = emitterEntities.find(stableId);
            if (it != emitterEntities.end() && entityAlive(it->second))
                return it->second;

            Entity entity = world.createEntity();
            world.addComponent(entity, TransformComponent{});
            emitterEntities[stableId] = entity;
            return entity;
        }

        TransformComponent& ensureTransform(Entity entity)
        {
            if (!world.hasComponent<TransformComponent>(entity))
                world.addComponent(entity, TransformComponent{});
            return world.getComponent<TransformComponent>(entity);
        }

        ParticleEmitterRuntimeComponent& ensureRuntime(Entity entity, const ParticleEmitterComponent& descriptor)
        {
            if (!world.hasComponent<ParticleEmitterRuntimeComponent>(entity))
            {
                ParticleEmitterRuntimeComponent runtime;
                runtime.rngState = descriptor.randomSeed == 0u ? entity.id + 1u : descriptor.randomSeed;
                runtime.particles.reserve(std::max(1u, descriptor.maxParticles));
                world.addComponent(entity, runtime);
            }
            return world.getComponent<ParticleEmitterRuntimeComponent>(entity);
        }

        void removeMissingEmitters(const ParticleEffectAsset& asset)
        {
            for (auto it = emitterEntities.begin(); it != emitterEntities.end();)
            {
                const bool exists = std::any_of(asset.emitters.begin(),
                                                asset.emitters.end(),
                                                [&](const ParticleEffectEmitter& emitter)
                                                {
                                                    return emitter.stableId == it->first &&
                                                           emitter.descriptor.enabled &&
                                                           emitter.compiledProgram.valid;
                                                });
                if (exists)
                {
                    ++it;
                    continue;
                }

                if (entityAlive(it->second))
                    world.destroyEntity(it->second);
                it = emitterEntities.erase(it);
            }
        }

        void destroyEmitter(const std::string& stableId)
        {
            auto it = emitterEntities.find(stableId);
            if (it == emitterEntities.end())
                return;

            if (entityAlive(it->second))
                world.destroyEntity(it->second);
            emitterEntities.erase(it);
        }

        void updateCamera(const ParticleEffectAsset& asset,
                          uint32_t width,
                          uint32_t height,
                          const EngineToolInputCaptureComponent* inputCapture)
        {
            if (cameraEntity == INVALID_ENTITY || !entityAlive(cameraEntity))
                createCamera();

            const glm::vec3 defaultTarget = asset.preview.cameraTarget;
            glm::vec3 defaultPosition = asset.preview.cameraPosition;
            if (glm::dot(defaultPosition - defaultTarget, defaultPosition - defaultTarget) <= 0.000001f)
                defaultPosition = defaultTarget + glm::vec3(0.0f, 1.0f, 4.0f);

            const float defaultDistance = std::max(0.001f, glm::length(defaultPosition - defaultTarget));
            if (!cameraState.initialized)
            {
                resetOrbitCamera(cameraState,
                                 defaultPosition,
                                 defaultTarget,
                                 0.12f,
                                 std::max(12.0f, defaultDistance * 6.0f));
            }
            else
            {
                cameraState.minDistance = 0.12f;
                cameraState.maxDistance = std::max(12.0f, defaultDistance * 6.0f);
                cameraState.distance = glm::clamp(cameraState.distance,
                                                  cameraState.minDistance,
                                                  cameraState.maxDistance);
            }

            if (inputCapture != nullptr)
                applyOrbitCameraInput(cameraState, *inputCapture, true);

            TransformComponent& transform = ensureTransform(cameraEntity);
            transform.position = orbitCameraPosition(cameraState);
            gts::transform::markDirty(world, cameraEntity);

            CameraDescriptionComponent& camera = world.getComponent<CameraDescriptionComponent>(cameraEntity);
            camera.target = cameraState.target;
            camera.up = {0.0f, 1.0f, 0.0f};
            camera.fov = glm::radians(70.0f);
            camera.aspectRatio = static_cast<float>(std::max(1u, width)) /
                                 static_cast<float>(std::max(1u, height));
            camera.active = true;
        }

        void setRuntimePlayback(float playbackTimeScale, bool paused)
        {
            world.forEach<ParticleEmitterRuntimeComponent>(
                [&](Entity, ParticleEmitterRuntimeComponent& runtime)
                {
                    runtime.playbackTimeScale = playbackTimeScale;
                    runtime.playbackPaused = paused;
                });
        }

        bool entityAlive(Entity entity) const
        {
            return entity != INVALID_ENTITY && world.hasComponent<TransformComponent>(entity);
        }
    };
}
