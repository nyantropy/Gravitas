#pragma once

#include "BoundsComponent.h"
#include "CameraDescriptionComponent.h"
#include "DirectionalLightComponent.h"
#include "ECSWorld.hpp"
#include "GlmConfig.h"
#include "GraphicsConstants.h"
#include "GtsScene.hpp"
#include "MaterialReferenceComponent.h"
#include "MaterialRuntime.h"
#include "RendererSceneFeature.h"
#include "StaticMeshComponent.h"
#include "TransformComponent.h"

class PbrValidationScene final : public GtsScene
{
public:
    void onLoad(EcsControllerContext& ctx,
                const GtsSceneTransitionData* = nullptr) override
    {
        gts::rendering::installRendererFeature(*this, ctx);
        spawnPbrGrid();
        spawnComparisonObjects();
        spawnDirectionalLight();
        spawnCamera(ctx.windowAspectRatio);
    }

    void onUpdateSimulation(const EcsSimulationContext& ctx) override
    {
        ecsWorld.updateSimulation(ctx);
    }

    void onUpdateControllers(const EcsControllerContext& ctx) override
    {
        ecsWorld.updateControllers(ctx);
    }

private:
    static constexpr const char* CubeMesh = "/models/cube.obj";
    static constexpr const char* NeutralTexture = "/textures/engine_debug_neutral.png";

    MaterialInstanceHandle createStandardSurface(const glm::vec4& baseColor,
                                                 float metallic,
                                                 float roughness,
                                                 bool transparent = false)
    {
        gts::rendering::MaterialRuntime& materials = gts::rendering::materialRuntime(ecsWorld);
        const MaterialInstance* defaultSurface =
            materials.getInstance(materials.defaultStandardSurfaceMaterial());

        MaterialInstance instance = defaultSurface != nullptr ? *defaultSurface : MaterialInstance{};
        instance.baseColor = baseColor;
        instance.baseColorTexture = MaterialTextureBinding::assetPath(
            GraphicsConstants::ENGINE_RESOURCES + NeutralTexture);
        instance.metallic = metallic;
        instance.roughness = roughness;
        instance.renderState.alphaMode = transparent ? MaterialAlphaMode::Blend : MaterialAlphaMode::Opaque;
        instance.renderState.depthWrite = !transparent;
        return materials.createInstance(instance);
    }

    MaterialInstanceHandle createUnlitMaterial(const glm::vec4& baseColor)
    {
        gts::rendering::MaterialRuntime& materials = gts::rendering::materialRuntime(ecsWorld);
        const MaterialInstance* defaultUnlit = materials.getInstance(materials.defaultMaterial());

        MaterialInstance instance = defaultUnlit != nullptr ? *defaultUnlit : MaterialInstance{};
        instance.baseColor = baseColor;
        instance.baseColorTexture = MaterialTextureBinding::assetPath(
            GraphicsConstants::ENGINE_RESOURCES + NeutralTexture);
        instance.renderState.alphaMode = MaterialAlphaMode::Opaque;
        instance.renderState.depthWrite = true;
        return materials.createInstance(instance);
    }

    Entity spawnCube(const glm::vec3& position,
                     const glm::vec3& scale,
                     MaterialInstanceHandle material)
    {
        Entity entity = ecsWorld.createEntity();

        StaticMeshComponent mesh;
        mesh.meshPath = GraphicsConstants::ENGINE_RESOURCES + CubeMesh;
        ecsWorld.addComponent(entity, mesh);

        TransformComponent transform;
        transform.position = position;
        transform.scale = scale;
        ecsWorld.addComponent(entity, transform);

        ecsWorld.addComponent(entity, MaterialReferenceComponent{material});
        ecsWorld.addComponent(entity, BoundsComponent{});
        return entity;
    }

    void spawnPbrGrid()
    {
        const float metallicValues[] = {0.0f, 0.25f, 0.5f, 0.75f, 1.0f};
        const float roughnessValues[] = {0.04f, 0.25f, 0.5f, 0.75f, 1.0f};
        const glm::vec3 origin{-4.8f, 1.6f, 0.0f};

        for (int row = 0; row < 5; ++row)
        {
            for (int column = 0; column < 5; ++column)
            {
                const float metallic = metallicValues[column];
                const float roughness = roughnessValues[row];
                const glm::vec4 color = glm::mix(
                    glm::vec4(0.8f, 0.28f, 0.12f, 1.0f),
                    glm::vec4(0.92f, 0.74f, 0.38f, 1.0f),
                    metallic);
                spawnCube(
                    origin + glm::vec3(column * 2.4f, -row * 1.2f, 0.0f),
                    {0.9f, 0.9f, 0.9f},
                    createStandardSurface(color, metallic, roughness));
            }
        }
    }

    void spawnComparisonObjects()
    {
        spawnCube(
            {8.0f, 0.2f, 0.0f},
            {0.8f, 1.8f, 0.55f},
            createStandardSurface({0.25f, 0.55f, 1.0f, 1.0f}, 0.0f, 0.35f));
        spawnCube(
            {8.0f, -1.8f, 0.0f},
            {0.9f, 0.9f, 0.9f},
            createUnlitMaterial({0.45f, 0.85f, 1.0f, 1.0f}));
        spawnCube(
            {-7.2f, -4.8f, 0.0f},
            {1.1f, 1.1f, 1.1f},
            createStandardSurface({0.9f, 0.45f, 0.15f, 0.45f}, 0.0f, 0.2f, true));
    }

    void spawnDirectionalLight()
    {
        Entity lightEntity = ecsWorld.createEntity();

        TransformComponent transform;
        transform.rotation = glm::vec3(glm::radians(-25.0f), glm::radians(35.0f), 0.0f);
        ecsWorld.addComponent(lightEntity, transform);

        DirectionalLightComponent light;
        light.active = true;
        light.color = {1.0f, 0.96f, 0.9f};
        light.intensity = 3.5f;
        light.ambientIntensity = 0.06f;
        ecsWorld.addComponent(lightEntity, light);
    }

    void spawnCamera(float aspectRatio)
    {
        Entity camera = ecsWorld.createEntity();

        CameraDescriptionComponent desc;
        desc.active = true;
        desc.fov = glm::radians(48.0f);
        desc.aspectRatio = aspectRatio;
        desc.nearClip = 0.1f;
        desc.farClip = 100.0f;
        ecsWorld.addComponent(camera, desc);

        TransformComponent transform;
        transform.position = glm::vec3(1.2f, -1.4f, 18.0f);
        ecsWorld.addComponent(camera, transform);
    }
};
