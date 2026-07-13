#pragma once

#include "BoundsComponent.h"
#include "CameraDescriptionComponent.h"
#include "DirectionalLightComponent.h"
#include "DynamicMeshComponent.h"
#include "ECSWorld.hpp"
#include "EnvironmentLightComponent.h"
#include "GlmConfig.h"
#include "GraphicsConstants.h"
#include "GtsScene.hpp"
#include "MaterialReferenceComponent.h"
#include "MaterialRuntime.h"
#include "PointLightComponent.h"
#include "RendererSceneFeature.h"
#include "SpotLightComponent.h"
#include "StaticMeshComponent.h"
#include "TransformComponent.h"
#include "TransformHierarchyHelpers.h"

class PbrValidationScene final : public GtsScene
{
public:
    void onLoad(EcsControllerContext& ctx,
                const GtsSceneTransitionData* = nullptr) override
    {
        gts::rendering::installRendererFeature(*this, ctx);
        spawnPbrGrid();
        spawnComparisonObjects();
        spawnTextureMapSamples();
        spawnLights();
        spawnEnvironment();
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
    static constexpr const char* CubeMesh = "/models/cube.gmesh";
    static constexpr const char* NeutralTexture = "/textures/engine_debug_neutral.png";
    static constexpr const char* ValidationBaseTexture =
        "/textures/engine_pbr_validation_base.png";
    static constexpr const char* ValidationMetallicRoughnessTexture =
        "/textures/engine_pbr_validation_metallic_roughness.png";
    static constexpr const char* ValidationNormalTexture =
        "/textures/engine_pbr_validation_normal.png";
    static constexpr const char* ValidationAoTexture =
        "/textures/engine_pbr_validation_ao.png";
    static constexpr const char* ValidationEmissiveTexture =
        "/textures/engine_pbr_validation_emissive.png";
    static constexpr const char* ValidationEnvironmentTexture =
        "/textures/engine_ibl_validation_environment.hdr";

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

    MaterialInstanceHandle createTextureMappedSurface(const glm::vec4& baseColor,
                                                      float metallic,
                                                      float roughness)
    {
        gts::rendering::MaterialRuntime& materials = gts::rendering::materialRuntime(ecsWorld);
        const MaterialInstance* defaultSurface =
            materials.getInstance(materials.defaultStandardSurfaceMaterial());

        MaterialInstance instance = defaultSurface != nullptr ? *defaultSurface : MaterialInstance{};
        instance.baseColor = baseColor;
        instance.baseColorTexture = MaterialTextureBinding::assetPath(
            GraphicsConstants::ENGINE_RESOURCES + ValidationBaseTexture);
        instance.metallic = metallic;
        instance.roughness = roughness;
        instance.metallicRoughnessTexture = MaterialTextureBinding::dataAssetPath(
            GraphicsConstants::ENGINE_RESOURCES + ValidationMetallicRoughnessTexture);
        instance.normalTexture = MaterialTextureBinding::dataAssetPath(
            GraphicsConstants::ENGINE_RESOURCES + ValidationNormalTexture);
        instance.ambientOcclusionTexture = MaterialTextureBinding::dataAssetPath(
            GraphicsConstants::ENGINE_RESOURCES + ValidationAoTexture);
        instance.emissiveTexture = MaterialTextureBinding::assetPath(
            GraphicsConstants::ENGINE_RESOURCES + ValidationEmissiveTexture);
        instance.normalScale = 0.85f;
        instance.ambientOcclusionStrength = 0.75f;
        instance.emissiveFactor = {0.55f, 0.65f, 1.0f};
        instance.emissiveStrength = 0.35f;
        instance.renderState.alphaMode = MaterialAlphaMode::Opaque;
        instance.renderState.depthWrite = true;
        return materials.createInstance(instance);
    }

    MaterialInstanceHandle createNormalMappedSurface(float normalScale)
    {
        gts::rendering::MaterialRuntime& materials = gts::rendering::materialRuntime(ecsWorld);
        const MaterialInstance* defaultSurface =
            materials.getInstance(materials.defaultStandardSurfaceMaterial());

        MaterialInstance instance = defaultSurface != nullptr ? *defaultSurface : MaterialInstance{};
        instance.baseColor = {0.72f, 0.72f, 0.78f, 1.0f};
        instance.baseColorTexture = MaterialTextureBinding::assetPath(
            GraphicsConstants::ENGINE_RESOURCES + NeutralTexture);
        instance.metallic = 0.0f;
        instance.roughness = 0.45f;
        instance.normalTexture = MaterialTextureBinding::dataAssetPath(
            GraphicsConstants::ENGINE_RESOURCES + ValidationNormalTexture);
        instance.normalScale = normalScale;
        return materials.createInstance(instance);
    }

    MaterialInstanceHandle createAoEmissiveSurface()
    {
        gts::rendering::MaterialRuntime& materials = gts::rendering::materialRuntime(ecsWorld);
        const MaterialInstance* defaultSurface =
            materials.getInstance(materials.defaultStandardSurfaceMaterial());

        MaterialInstance instance = defaultSurface != nullptr ? *defaultSurface : MaterialInstance{};
        instance.baseColor = {0.18f, 0.2f, 0.24f, 1.0f};
        instance.baseColorTexture = MaterialTextureBinding::assetPath(
            GraphicsConstants::ENGINE_RESOURCES + ValidationBaseTexture);
        instance.metallic = 0.0f;
        instance.roughness = 0.75f;
        instance.ambientOcclusionTexture = MaterialTextureBinding::dataAssetPath(
            GraphicsConstants::ENGINE_RESOURCES + ValidationAoTexture);
        instance.ambientOcclusionStrength = 1.0f;
        instance.emissiveTexture = MaterialTextureBinding::assetPath(
            GraphicsConstants::ENGINE_RESOURCES + ValidationEmissiveTexture);
        instance.emissiveFactor = {1.0f, 0.75f, 0.45f};
        instance.emissiveStrength = 1.4f;
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

    Entity spawnUnlitAttributeQuad(const glm::vec3& position,
                                   const glm::vec3& scale,
                                   MaterialInstanceHandle material)
    {
        Entity entity = ecsWorld.createEntity();

        DynamicMeshComponent mesh;
        mesh.vertices = {
            Vertex{{-0.55f, -0.55f, 0.0f}, glm::vec4{1.0f}, {0.0f, 0.0f}},
            Vertex{{ 0.55f, -0.55f, 0.0f}, glm::vec4{1.0f}, {1.0f, 0.0f}},
            Vertex{{ 0.55f,  0.55f, 0.0f}, glm::vec4{1.0f}, {1.0f, 1.0f}},
            Vertex{{-0.55f,  0.55f, 0.0f}, glm::vec4{1.0f}, {0.0f, 1.0f}}
        };
        mesh.indices = {0, 1, 2, 0, 2, 3};
        mesh.geometryVersion = 1;
        mesh.sourceAttributes = UnlitVertexAttributes;
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

    void spawnTextureMapSamples()
    {
        const MaterialInstanceHandle allMaps =
            createTextureMappedSurface({1.0f, 1.0f, 1.0f, 1.0f}, 0.7f, 0.65f);
        const MaterialInstanceHandle sharedNormal = createNormalMappedSurface(1.0f);
        const MaterialInstanceHandle strongNormal = createNormalMappedSurface(1.35f);
        const MaterialInstanceHandle aoEmissive = createAoEmissiveSurface();

        spawnCube({-4.8f, -5.4f, 0.0f}, {0.85f, 0.85f, 0.85f}, allMaps);
        spawnCube({-2.4f, -5.4f, 0.0f}, {0.75f, 1.15f, 0.75f}, sharedNormal);
        spawnCube({-0.8f, -5.4f, 0.0f}, {1.1f, 0.75f, 0.75f}, sharedNormal);
        spawnCube({1.6f, -5.4f, 0.0f}, {0.7f, 1.65f, 0.45f}, strongNormal);
        spawnCube({4.0f, -5.4f, 0.0f}, {0.9f, 0.9f, 0.9f}, aoEmissive);
        spawnUnlitAttributeQuad(
            {6.4f, -5.4f, 0.0f},
            {1.25f, 1.25f, 1.25f},
            strongNormal);
    }

    void spawnLights()
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

        const glm::vec3 pointPositions[] = {
            {-5.5f, 2.5f, 3.0f},
            {-0.5f, 3.8f, 2.4f},
            {4.5f, 2.0f, 3.2f},
            {8.5f, -1.0f, 2.2f}
        };
        const glm::vec3 pointColors[] = {
            {1.0f, 0.28f, 0.18f},
            {0.2f, 0.55f, 1.0f},
            {0.95f, 0.75f, 0.25f},
            {0.35f, 1.0f, 0.55f}
        };
        for (int i = 0; i < 4; ++i)
        {
            Entity point = ecsWorld.createEntity();
            TransformComponent pointTransform;
            pointTransform.position = pointPositions[i];
            ecsWorld.addComponent(point, pointTransform);

            PointLightComponent pointLight;
            pointLight.color = pointColors[i];
            pointLight.intensity = 6.0f;
            pointLight.range = 6.5f;
            pointLight.priority = 10;
            ecsWorld.addComponent(point, pointLight);
        }

        Entity spot = ecsWorld.createEntity();
        TransformComponent spotTransform;
        spotTransform.position = glm::vec3(-2.5f, -4.5f, 5.0f);
        spotTransform.rotation = glm::vec3(glm::radians(18.0f), glm::radians(-18.0f), 0.0f);
        ecsWorld.addComponent(spot, spotTransform);

        SpotLightComponent spotLight;
        spotLight.color = {1.0f, 0.9f, 0.65f};
        spotLight.intensity = 18.0f;
        spotLight.range = 10.0f;
        spotLight.innerConeAngle = glm::radians(15.0f);
        spotLight.outerConeAngle = glm::radians(28.0f);
        spotLight.priority = 20;
        ecsWorld.addComponent(spot, spotLight);

        Entity rig = ecsWorld.createEntity();
        TransformComponent rigTransform;
        rigTransform.position = glm::vec3(5.5f, -4.8f, 0.0f);
        rigTransform.rotation = glm::vec3(0.0f, glm::radians(25.0f), 0.0f);
        ecsWorld.addComponent(rig, rigTransform);

        Entity parentedSpot = ecsWorld.createEntity();
        TransformComponent parentedSpotTransform;
        parentedSpotTransform.position = glm::vec3(0.0f, 0.0f, 4.5f);
        parentedSpotTransform.rotation = glm::vec3(glm::radians(12.0f), glm::radians(8.0f), 0.0f);
        ecsWorld.addComponent(parentedSpot, parentedSpotTransform);
        SpotLightComponent parentedSpotLight = spotLight;
        parentedSpotLight.color = {0.55f, 0.7f, 1.0f};
        parentedSpotLight.priority = 18;
        ecsWorld.addComponent(parentedSpot, parentedSpotLight);
        gts::transform::attachToParent(ecsWorld, parentedSpot, rig);

        Entity parentedPoint = ecsWorld.createEntity();
        TransformComponent parentedPointTransform;
        parentedPointTransform.position = glm::vec3(-1.0f, 1.2f, 2.2f);
        ecsWorld.addComponent(parentedPoint, parentedPointTransform);
        PointLightComponent parentedPointLight;
        parentedPointLight.color = {1.0f, 0.35f, 0.8f};
        parentedPointLight.intensity = 7.0f;
        parentedPointLight.range = 5.0f;
        parentedPointLight.priority = 12;
        ecsWorld.addComponent(parentedPoint, parentedPointLight);
        gts::transform::attachToParent(ecsWorld, parentedPoint, rig);

        for (uint32_t i = 0; i < gts::rendering::MaxPointLights + 4; ++i)
        {
            Entity extra = ecsWorld.createEntity();
            TransformComponent extraTransform;
            extraTransform.position = glm::vec3(-18.0f + static_cast<float>(i), -12.0f, -8.0f);
            ecsWorld.addComponent(extra, extraTransform);

            PointLightComponent extraLight;
            extraLight.color = {0.25f, 0.25f, 0.35f};
            extraLight.intensity = 0.5f;
            extraLight.range = 2.0f;
            extraLight.priority = -100;
            ecsWorld.addComponent(extra, extraLight);
        }

        for (uint32_t i = 0; i < gts::rendering::MaxSpotLights + 2; ++i)
        {
            Entity extra = ecsWorld.createEntity();
            TransformComponent extraTransform;
            extraTransform.position = glm::vec3(-18.0f + static_cast<float>(i), -16.0f, -10.0f);
            ecsWorld.addComponent(extra, extraTransform);

            SpotLightComponent extraLight;
            extraLight.color = {0.2f, 0.2f, 0.25f};
            extraLight.intensity = 0.5f;
            extraLight.range = 2.0f;
            extraLight.priority = -100;
            ecsWorld.addComponent(extra, extraLight);
        }
    }

    void spawnEnvironment()
    {
        Entity environment = ecsWorld.createEntity();
        EnvironmentLightComponent light;
        light.environmentPath = GraphicsConstants::ENGINE_RESOURCES + ValidationEnvironmentTexture;
        light.intensity = 0.7f;
        light.rotationRadians = glm::radians(18.0f);
        light.priority = 10;
        ecsWorld.addComponent(environment, light);

        Entity disabledFallback = ecsWorld.createEntity();
        EnvironmentLightComponent disabled;
        disabled.environmentPath = {};
        disabled.intensity = 2.0f;
        disabled.enabled = false;
        disabled.priority = 100;
        ecsWorld.addComponent(disabledFallback, disabled);
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
