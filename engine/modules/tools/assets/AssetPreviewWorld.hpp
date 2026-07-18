#pragma once

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "ActiveCameraViewSystem.hpp"
#include "AssetManifest.h"
#include "BoundsComponent.h"
#include "CameraBindingLifecycle.h"
#include "CameraBindingSystem.hpp"
#include "CameraDescriptionComponent.h"
#include "CameraGpuComponent.h"
#include "CameraGpuSystem.hpp"
#include "CameraLifecycleSystem.hpp"
#include "DirectionalLightComponent.h"
#include "DynamicMeshComponent.h"
#include "ECSWorld.hpp"
#include "EcsControllerContext.hpp"
#include "EditorPreviewRenderData.h"
#include "FrustumCullingStrategy.h"
#include "GlmConfig.h"
#include "IResourceProvider.hpp"
#include "MaterialReferenceComponent.h"
#include "MaterialReferenceHelpers.h"
#include "PointLightComponent.h"
#include "RenderPipeline.h"
#include "RendererGeometrySceneFeature.h"
#include "StaticMeshComponent.h"
#include "TimeContext.h"
#include "TransformComponent.h"
#include "TransformDirtyHelpers.h"
#include "TransformSceneFeature.h"
#include "Vertex.h"

namespace gts::tools
{
    class AssetPreviewWorld
    {
    public:
        AssetPreviewWorld()
            : renderPipeline(std::make_unique<FrustumCullingStrategy>(true))
        {
        }

        ~AssetPreviewWorld()
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
            installed = true;

            createCamera();
            createGrid();
            createLights();
        }

        void destroy()
        {
            cameraEntity = INVALID_ENTITY;
            gridEntity = INVALID_ENTITY;
            keyLightEntity = INVALID_ENTITY;
            fillLightEntity = INVALID_ENTITY;
            assetEntity = INVALID_ENTITY;
            currentSignature.clear();
            currentManifest = {};
            hasAsset = false;
            world.clear();
            renderPipeline.resetSceneState();
            installed = false;
            resourceProvider = nullptr;
        }

        void syncAsset(const std::string& manifestPath, const AssetManifest& manifest)
        {
            if (!installed || manifest.modelPath.empty())
                return;

            const std::string signature = assetSignature(manifestPath, manifest);
            currentManifest = manifest;
            hasAsset = true;

            if (assetEntity != INVALID_ENTITY && entityAlive(assetEntity) && signature == currentSignature)
            {
                updateAssetTransform(manifest);
                return;
            }

            destroyAssetEntity();
            currentSignature = signature;
            createAssetEntity(manifest);
        }

        EditorPreviewRenderData buildFrame(float dt, uint32_t width, uint32_t height)
        {
            EditorPreviewRenderData data;
            if (!installed || resourceProvider == nullptr || !hasAsset)
                return data;

            updateCamera(width, height);

            TimeContext previewTime;
            previewTime.unscaledDeltaTime = std::clamp(dt, 0.0f, 0.1f);
            previewTime.deltaTime = previewTime.unscaledDeltaTime;
            previewTime.timeScale = 1.0f;
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
            data.particleData = {};
            return data;
        }

    private:
        struct FramedBounds
        {
            glm::vec3 min = {-0.5f, 0.0f, -0.5f};
            glm::vec3 max = {0.5f, 1.0f, 0.5f};
            glm::vec3 center = {0.0f, 0.5f, 0.0f};
            float radius = 0.75f;
        };

        ECSWorld world;
        RenderPipeline renderPipeline;
        IResourceProvider* resourceProvider = nullptr;
        Entity cameraEntity = INVALID_ENTITY;
        Entity gridEntity = INVALID_ENTITY;
        Entity keyLightEntity = INVALID_ENTITY;
        Entity fillLightEntity = INVALID_ENTITY;
        Entity assetEntity = INVALID_ENTITY;
        AssetManifest currentManifest;
        std::string currentSignature;
        bool installed = false;
        bool hasAsset = false;
        uint64_t frame = 0;

        void abandon()
        {
            cameraEntity = INVALID_ENTITY;
            gridEntity = INVALID_ENTITY;
            keyLightEntity = INVALID_ENTITY;
            fillLightEntity = INVALID_ENTITY;
            assetEntity = INVALID_ENTITY;
            currentSignature.clear();
            hasAsset = false;
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
            transform.position = {1.8f, 1.25f, 2.4f};

            CameraDescriptionComponent camera;
            camera.target = {0.0f, 0.55f, 0.0f};
            camera.up = {0.0f, 1.0f, 0.0f};
            camera.fov = glm::radians(54.0f);
            camera.nearClip = 0.03f;
            camera.farClip = 120.0f;
            camera.aspectRatio = 1.0f;
            camera.active = true;

            world.addComponent(cameraEntity, transform);
            world.addComponent(cameraEntity, camera);
        }

        void createLights()
        {
            keyLightEntity = world.createEntity();
            TransformComponent keyTransform;
            keyTransform.rotation = {-0.58f, 0.72f, 0.0f};
            world.addComponent(keyLightEntity, keyTransform);

            DirectionalLightComponent keyLight;
            keyLight.color = {1.0f, 0.93f, 0.82f};
            keyLight.intensity = 2.1f;
            keyLight.ambientIntensity = 0.24f;
            keyLight.priority = 10;
            keyLight.active = true;
            world.addComponent(keyLightEntity, keyLight);

            fillLightEntity = world.createEntity();
            TransformComponent fillTransform;
            fillTransform.position = {-1.8f, 1.0f, 1.4f};
            world.addComponent(fillLightEntity, fillTransform);

            PointLightComponent fillLight;
            fillLight.color = {0.58f, 0.72f, 1.0f};
            fillLight.intensity = 0.65f;
            fillLight.range = 4.5f;
            fillLight.priority = 4;
            world.addComponent(fillLightEntity, fillLight);
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
            const int halfCells = 4;
            const float extent = static_cast<float>(halfCells);
            const float thickness = 0.014f;
            addFloorQuad(mesh.vertices, mesh.indices, 2.65f, -0.035f, {0.028f, 0.034f, 0.040f});
            for (int i = -halfCells; i <= halfCells; ++i)
            {
                const float p = static_cast<float>(i);
                const glm::vec3 color = i == 0 ? glm::vec3(0.33f, 0.48f, 0.56f)
                                               : glm::vec3(0.105f, 0.125f, 0.145f);
                addLineRect(mesh.vertices, mesh.indices, {-extent, 0.0f, p}, {extent, 0.0f, p}, thickness, color);
                addLineRect(mesh.vertices, mesh.indices, {p, 0.0f, -extent}, {p, 0.0f, extent}, thickness, color);
            }

            gts::rendering::UnlitMaterialDescriptor material;
            material.baseColor = {1.0f, 1.0f, 1.0f, 0.80f};
            material.vertexColorOnly = true;
            material.doubleSided = true;
            material.depthWrite = true;

            world.addComponent(gridEntity, TransformComponent{});
            world.addComponent(gridEntity, mesh);
            world.addComponent(gridEntity, gts::rendering::sharedUnlitMaterialReference(world, material));
        }

        void createAssetEntity(const AssetManifest& manifest)
        {
            assetEntity = world.createEntity();

            TransformComponent transform = assetTransform(manifest);
            StaticMeshComponent mesh;
            mesh.meshPath = resourceAssetPath(manifest.modelPath);
            mesh.useMeshMaterials = manifest.materialMode == AssetMaterialMode::CookedMeshMaterials;

            world.addComponent(assetEntity, transform);
            world.addComponent(assetEntity, mesh);
            world.addComponent(assetEntity, materialReferenceForManifest(manifest));
            world.addComponent(assetEntity, manifest.bounds);
            gts::transform::markDirty(world, assetEntity);
        }

        void updateAssetTransform(const AssetManifest& manifest)
        {
            if (assetEntity == INVALID_ENTITY || !entityAlive(assetEntity))
                return;

            TransformComponent& transform = world.getComponent<TransformComponent>(assetEntity);
            transform = assetTransform(manifest);
            gts::transform::markDirty(world, assetEntity);
            if (world.hasComponent<BoundsComponent>(assetEntity))
                world.getComponent<BoundsComponent>(assetEntity) = manifest.bounds;
        }

        void destroyAssetEntity()
        {
            if (assetEntity != INVALID_ENTITY && entityAlive(assetEntity))
                world.destroyEntity(assetEntity);
            assetEntity = INVALID_ENTITY;
        }

        MaterialReferenceComponent materialReferenceForManifest(const AssetManifest& manifest)
        {
            if (manifest.materialMode == AssetMaterialMode::CookedMeshMaterials)
            {
                return MaterialReferenceComponent{
                    gts::rendering::materialRuntime(world).defaultStandardSurfaceMaterial()};
            }

            gts::rendering::UnlitMaterialDescriptor material;
            material.texturePath = resourceAssetPath(manifest.fallbackTexturePath);
            material.baseColor = {1.0f, 1.0f, 1.0f, 1.0f};
            material.doubleSided = true;
            material.depthWrite = true;
            return gts::rendering::sharedUnlitMaterialReference(world, material);
        }

        void updateCamera(uint32_t width, uint32_t height)
        {
            if (cameraEntity == INVALID_ENTITY || !entityAlive(cameraEntity))
                createCamera();

            const FramedBounds bounds = framedBounds(currentManifest);
            const float manifestDistance = std::max(0.0f, currentManifest.preview.cameraDistance);
            const float distance = std::max(manifestDistance, bounds.radius * 2.35f + 0.35f);
            const glm::vec3 viewDirection = glm::normalize(glm::vec3(0.88f, 0.42f, 1.08f));

            TransformComponent& transform = world.getComponent<TransformComponent>(cameraEntity);
            transform.position = bounds.center + viewDirection * distance;
            gts::transform::markDirty(world, cameraEntity);

            CameraDescriptionComponent& camera = world.getComponent<CameraDescriptionComponent>(cameraEntity);
            camera.target = bounds.center;
            camera.up = {0.0f, 1.0f, 0.0f};
            camera.fov = glm::radians(54.0f);
            camera.nearClip = std::max(0.01f, distance - bounds.radius * 4.0f);
            camera.farClip = std::max(30.0f, distance + bounds.radius * 5.0f);
            camera.aspectRatio = static_cast<float>(std::max(1u, width)) /
                                 static_cast<float>(std::max(1u, height));
            camera.active = true;
        }

        static TransformComponent assetTransform(const AssetManifest& manifest)
        {
            TransformComponent transform;
            transform.scale = manifest.scale;
            transform.position = assetWorldOffset(manifest);
            return transform;
        }

        static glm::vec3 assetWorldOffset(const AssetManifest& manifest)
        {
            const glm::vec3 scaledMin = glm::min(manifest.bounds.min * manifest.scale,
                                                 manifest.bounds.max * manifest.scale);
            const glm::vec3 scaledMax = glm::max(manifest.bounds.min * manifest.scale,
                                                 manifest.bounds.max * manifest.scale);
            const glm::vec3 center = (scaledMin + scaledMax) * 0.5f;

            glm::vec3 offset = {-center.x, -center.y, -center.z};
            if (manifest.baseAtGround)
                offset.y = -scaledMin.y;
            return offset;
        }

        static FramedBounds framedBounds(const AssetManifest& manifest)
        {
            const glm::vec3 scaledMin = glm::min(manifest.bounds.min * manifest.scale,
                                                 manifest.bounds.max * manifest.scale);
            const glm::vec3 scaledMax = glm::max(manifest.bounds.min * manifest.scale,
                                                 manifest.bounds.max * manifest.scale);
            const glm::vec3 offset = assetWorldOffset(manifest);

            FramedBounds bounds;
            bounds.min = scaledMin + offset;
            bounds.max = scaledMax + offset;
            bounds.center = (bounds.min + bounds.max) * 0.5f;

            const glm::vec3 size = glm::max(bounds.max - bounds.min,
                                            glm::vec3(0.10f, 0.10f, 0.10f));
            bounds.radius = std::max(0.35f, glm::length(size) * 0.5f);
            return bounds;
        }

        static std::string resourceAssetPath(const std::string& logicalPath)
        {
            if (logicalPath.empty())
                return {};

            const std::filesystem::path path(logicalPath);
            if (path.is_absolute())
                return path.generic_string();

            const std::string normalized = path.lexically_normal().generic_string();
            if (normalized.rfind("resources/", 0) == 0 ||
                normalized.rfind("./", 0) == 0 ||
                normalized.rfind("../", 0) == 0)
            {
                return normalized;
            }

            return (std::filesystem::path("resources/assets") / normalized).generic_string();
        }

        static std::string assetSignature(const std::string& manifestPath, const AssetManifest& manifest)
        {
            return manifestPath + "|" +
                manifest.modelPath + "|" +
                manifest.fallbackTexturePath + "|" +
                std::to_string(static_cast<int>(manifest.materialMode)) + "|" +
                vec3Signature(manifest.scale) + "|" +
                vec3Signature(manifest.bounds.min) + "|" +
                vec3Signature(manifest.bounds.max) + "|" +
                std::to_string(manifest.baseAtGround) + "|" +
                std::to_string(manifest.preview.cameraDistance);
        }

        static std::string vec3Signature(const glm::vec3& value)
        {
            return std::to_string(value.x) + "," +
                std::to_string(value.y) + "," +
                std::to_string(value.z);
        }

        bool entityAlive(Entity entity) const
        {
            return entity != INVALID_ENTITY && world.hasComponent<TransformComponent>(entity);
        }
    };
}
