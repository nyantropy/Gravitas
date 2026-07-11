#include <cmath>
#include <cstddef>
#include <cstdio>
#include <limits>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include "BitmapFont.h"
#include "CameraUBO.h"
#include "CameraGpuComponent.h"
#include "DirectionalLightComponent.h"
#include "ECSWorld.hpp"
#include "EcsControllerContext.hpp"
#include "IResourceProvider.hpp"
#include "LightExtraction.h"
#include "LightingFrameData.h"
#include "MaterialRuntime.h"
#include "PointLightComponent.h"
#include "RenderExtractionSnapshotBuilder.hpp"
#include "SpotLightComponent.h"
#include "TransformComponent.h"
#include "TransformHierarchyHelpers.h"
#include "TransformMatrixHelpers.h"
#include "TransformSceneFeature.h"
#include "Vertex.h"

namespace
{
    bool require(bool condition, const char* message)
    {
        if (condition)
            return true;

        std::fprintf(stderr, "FAIL: %s\n", message);
        return false;
    }

    bool near(float lhs, float rhs, float epsilon = 0.0005f)
    {
        return std::abs(lhs - rhs) <= epsilon;
    }

    bool nearVec3(const glm::vec3& lhs, const glm::vec3& rhs, float epsilon = 0.0005f)
    {
        return near(lhs.x, rhs.x, epsilon)
            && near(lhs.y, rhs.y, epsilon)
            && near(lhs.z, rhs.z, epsilon);
    }

    bool nearVec4(const glm::vec4& lhs, const glm::vec4& rhs, float epsilon = 0.0005f)
    {
        return near(lhs.x, rhs.x, epsilon)
            && near(lhs.y, rhs.y, epsilon)
            && near(lhs.z, rhs.z, epsilon)
            && near(lhs.w, rhs.w, epsilon);
    }

    bool finiteVec3(const glm::vec3& value)
    {
        return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
    }

    struct FakeResourceProvider : IResourceProvider
    {
        texture_id_type nextTexture = 100;
        ssbo_id_type nextSlot = 0;
        std::unordered_map<std::string, texture_id_type> textures;
        std::unordered_map<std::string, TextureColorSpace> textureColorSpaces;

        mesh_id_type requestMesh(const std::string&) override
        {
            return 1;
        }

        mesh_id_type getSharedQuadMesh(float, float) override
        {
            return 1;
        }

        mesh_id_type uploadProceduralMesh(mesh_id_type existingId,
                                          const std::vector<Vertex>&,
                                          const std::vector<uint32_t>&,
                                          VertexAttributeFlags = LegacyUnlitVertexAttributes) override
        {
            return existingId == 0 ? 1 : existingId;
        }

        void releaseProceduralMesh(mesh_id_type) override
        {
        }

        texture_id_type requestTexture(const std::string& path) override
        {
            auto [it, inserted] = textures.emplace(path, nextTexture);
            if (inserted)
                ++nextTexture;
            return it->second;
        }

        texture_id_type requestTexture(const std::string& path, TextureColorSpace colorSpace) override
        {
            textureColorSpaces[path] = colorSpace;
            return requestTexture(path);
        }

        texture_id_type requestClampedTexture(const std::string& path) override
        {
            return requestTexture(path);
        }

        texture_id_type requestPixelTexture(const std::string& path) override
        {
            return requestTexture(path);
        }

        TextureDimensions getTextureDimensions(texture_id_type) const override
        {
            return {16, 16};
        }

        font_id_type requestFont(const std::string&) override
        {
            return 1;
        }

        const BitmapFont* getFont(font_id_type) const override
        {
            return nullptr;
        }

        view_id_type requestCameraBuffer() override
        {
            return 1;
        }

        void releaseCameraBuffer(view_id_type) override
        {
        }

        void uploadCameraView(view_id_type, const glm::mat4&, const glm::mat4&) override
        {
        }

        ssbo_id_type requestObjectSlot() override
        {
            return nextSlot++;
        }

        void releaseObjectSlot(ssbo_id_type) override
        {
        }
    };

    Entity createTransformEntity(ECSWorld& world,
                                 const glm::vec3& position = {0.0f, 0.0f, 0.0f},
                                 const glm::vec3& rotation = {0.0f, 0.0f, 0.0f},
                                 const glm::vec3& scale = {1.0f, 1.0f, 1.0f})
    {
        Entity entity = world.createEntity();
        TransformComponent transform;
        transform.position = position;
        transform.rotation = rotation;
        transform.scale = scale;
        world.addComponent(entity, transform);
        return entity;
    }

    void resolveTransforms(ECSWorld& world)
    {
        EcsControllerContext ctx{world};
        world.updateControllers(ctx);
    }

    MaterialInstance makeInstance(MaterialDefinitionHandle definition,
                                  const glm::vec4& color = {1.0f, 1.0f, 1.0f, 1.0f})
    {
        MaterialInstance instance;
        instance.definition = definition;
        instance.baseColor = color;
        instance.baseColorTexture = MaterialTextureBinding::assetPath("textures/test.png");
        instance.renderState.alphaMode = MaterialAlphaMode::Opaque;
        return instance;
    }

    bool directionalLightSelectionIsPriorityBounded()
    {
        ECSWorld world;
        gts::transform::installTransformFeature(world);

        Entity inactive = createTransformEntity(world);
        DirectionalLightComponent inactiveLight;
        inactiveLight.active = false;
        inactiveLight.color = {1.0f, 0.0f, 0.0f};
        world.addComponent(inactive, inactiveLight);

        Entity active = createTransformEntity(world);
        DirectionalLightComponent activeLight;
        activeLight.active = true;
        activeLight.color = {0.0f, 1.0f, 0.0f};
        activeLight.intensity = 2.0f;
        activeLight.priority = 4;
        world.addComponent(active, activeLight);

        Entity higherPriority = createTransformEntity(world);
        DirectionalLightComponent highLight;
        highLight.active = true;
        highLight.color = {0.0f, 0.0f, 1.0f};
        highLight.intensity = 3.0f;
        highLight.priority = 9;
        world.addComponent(higherPriority, highLight);

        resolveTransforms(world);
        const gts::rendering::LightingFrameData frame =
            gts::rendering::extractLightingFrameData(world, {0.0f, 0.0f, 0.0f});

        return require(frame.directionalCount == 2, "two active directional lights are selected")
            && require(nearVec3(frame.directional[0].color, {0.0f, 0.0f, 1.0f}),
                       "directional lights are priority-sorted")
            && require(near(frame.directional[0].intensity, 3.0f),
                       "directional light intensity is copied to frame data")
            && require(frame.diagnostics.totalDirectionalLights == 3,
                       "directional diagnostics count found lights")
            && require(frame.diagnostics.droppedDirectionalLights == 0,
                       "directional selection reports no drops within capacity");
    }

    bool noActiveLightFallsBackToAmbientOnly()
    {
        ECSWorld world;
        gts::transform::installTransformFeature(world);

        Entity lightEntity = createTransformEntity(world);
        DirectionalLightComponent light;
        light.active = false;
        light.intensity = 8.0f;
        world.addComponent(lightEntity, light);

        resolveTransforms(world);
        const gts::rendering::LightingFrameData frame =
            gts::rendering::extractLightingFrameData(world, {0.0f, 0.0f, 0.0f});

        return require(frame.directionalCount == 0, "no active light reports empty directional frame data")
            && require(near(frame.ambientIntensity, 0.12f), "no-light fallback keeps ambient contribution");
    }

    bool parentedLightUsesResolvedWorldTransform()
    {
        ECSWorld world;
        gts::transform::installTransformFeature(world);

        constexpr float HalfPi = 1.57079632679f;
        Entity parent = createTransformEntity(
            world,
            {0.0f, 0.0f, 0.0f},
            {0.0f, HalfPi, 0.0f});
        Entity child = createTransformEntity(world);
        DirectionalLightComponent light;
        light.active = true;
        world.addComponent(child, light);
        require(gts::transform::attachToParent(world, child, parent), "attach light child to parent");

        resolveTransforms(world);
        const gts::rendering::LightingFrameData frame =
            gts::rendering::extractLightingFrameData(world, {0.0f, 0.0f, 0.0f});

        return require(frame.directionalCount == 1, "parented light is selected")
            && require(nearVec3(frame.directional[0].directionToLight, {1.0f, 0.0f, 0.0f}),
                       "light direction is resolved from parented world transform");
    }

    bool lightMutationUpdatesExtractedFrameData()
    {
        ECSWorld world;
        gts::transform::installTransformFeature(world);

        Entity lightEntity = createTransformEntity(world);
        DirectionalLightComponent light;
        light.active = true;
        light.intensity = 1.0f;
        world.addComponent(lightEntity, light);
        resolveTransforms(world);

        auto first = gts::rendering::extractLightingFrameData(world, {0.0f, 0.0f, 0.0f});
        auto& editable = world.getComponent<DirectionalLightComponent>(lightEntity);
        editable.intensity = 3.5f;
        editable.color = {0.25f, 0.5f, 0.75f};
        auto second = gts::rendering::extractLightingFrameData(world, {0.0f, 0.0f, 0.0f});

        return require(first.directionalCount == 1 && near(first.directional[0].intensity, 1.0f),
                       "first light extraction sees initial intensity")
            && require(second.directionalCount == 1 && near(second.directional[0].intensity, 3.5f),
                       "light intensity mutation reaches extraction")
            && require(nearVec3(second.directional[0].color, {0.25f, 0.5f, 0.75f}),
                       "light color mutation reaches extraction");
    }

    bool pointAndSpotLightsUseResolvedWorldTransforms()
    {
        ECSWorld world;
        gts::transform::installTransformFeature(world);

        Entity point = createTransformEntity(world, {2.0f, 3.0f, 4.0f});
        PointLightComponent pointLight;
        pointLight.color = {0.25f, 0.5f, 1.0f};
        pointLight.range = 8.0f;
        world.addComponent(point, pointLight);

        constexpr float HalfPi = 1.57079632679f;
        Entity parent = createTransformEntity(world, {0.0f, 0.0f, 0.0f}, {0.0f, HalfPi, 0.0f});
        Entity spot = createTransformEntity(world);
        SpotLightComponent spotLight;
        spotLight.range = 6.0f;
        spotLight.innerConeAngle = glm::radians(10.0f);
        spotLight.outerConeAngle = glm::radians(25.0f);
        world.addComponent(spot, spotLight);
        require(gts::transform::attachToParent(world, spot, parent), "attach spot light to parent");

        resolveTransforms(world);
        const gts::rendering::LightingFrameData frame =
            gts::rendering::extractLightingFrameData(world, {0.0f, 0.0f, 0.0f});

        return require(frame.pointCount == 1, "point light is extracted")
            && require(nearVec3(frame.point[0].position, {2.0f, 3.0f, 4.0f}),
                       "point light position comes from world transform")
            && require(frame.spotCount == 1, "spot light is extracted")
            && require(nearVec3(frame.spot[0].directionFromLight, {-1.0f, 0.0f, 0.0f}),
                       "spot cone direction comes from resolved world transform")
            && require(frame.spot[0].innerConeCos > frame.spot[0].outerConeCos,
                       "spot cone angles are converted to cosine thresholds");
    }

    bool localLightSelectionIsPriorityDistanceAndCapacityBounded()
    {
        ECSWorld world;
        gts::transform::installTransformFeature(world);

        for (uint32_t i = 0; i < gts::rendering::MaxPointLights + 2; ++i)
        {
            Entity entity = createTransformEntity(world, {static_cast<float>(i), 0.0f, 0.0f});
            PointLightComponent light;
            light.priority = 0;
            light.range = 4.0f;
            world.addComponent(entity, light);
        }

        Entity farHighPriority = createTransformEntity(world, {100.0f, 0.0f, 0.0f});
        PointLightComponent highPriority;
        highPriority.priority = 100;
        highPriority.range = 5.0f;
        highPriority.color = {1.0f, 0.0f, 0.0f};
        world.addComponent(farHighPriority, highPriority);

        resolveTransforms(world);
        const gts::rendering::LightingFrameData first =
            gts::rendering::extractLightingFrameData(world, {0.0f, 0.0f, 0.0f});
        const gts::rendering::LightingFrameData second =
            gts::rendering::extractLightingFrameData(world, {0.0f, 0.0f, 0.0f});

        return require(first.pointCount == gts::rendering::MaxPointLights,
                       "point light selection is bounded by capacity")
            && require(first.diagnostics.droppedPointLights == 3,
                       "point light diagnostics report capacity drops")
            && require(nearVec3(first.point[0].color, {1.0f, 0.0f, 0.0f}),
                       "priority outranks camera distance")
            && require(nearVec3(first.point[1].position, {0.0f, 0.0f, 0.0f}),
                       "camera distance orders equal-priority local lights")
            && require(second.pointCount == first.pointCount &&
                       nearVec3(second.point[0].position, first.point[0].position) &&
                       nearVec3(second.point[1].position, first.point[1].position),
                       "repeated light extraction is deterministic");
    }

    bool invalidLightValuesAreSanitized()
    {
        ECSWorld world;
        gts::transform::installTransformFeature(world);

        Entity point = createTransformEntity(world);
        PointLightComponent pointLight;
        pointLight.color = {-1.0f, 0.5f, std::numeric_limits<float>::quiet_NaN()};
        pointLight.intensity = -2.0f;
        pointLight.range = -10.0f;
        world.addComponent(point, pointLight);

        Entity spot = createTransformEntity(world);
        SpotLightComponent spotLight;
        spotLight.innerConeAngle = glm::radians(35.0f);
        spotLight.outerConeAngle = glm::radians(10.0f);
        spotLight.range = 0.0f;
        world.addComponent(spot, spotLight);

        resolveTransforms(world);
        const gts::rendering::LightingFrameData frame =
            gts::rendering::extractLightingFrameData(world, {0.0f, 0.0f, 0.0f});

        return require(frame.pointCount == 1 && frame.spotCount == 1,
                       "invalid but enabled local lights are still extracted with sanitized values")
            && require(near(frame.point[0].intensity, 0.0f), "negative point intensity clamps to zero")
            && require(near(frame.point[0].range, gts::rendering::MinLightRange),
                       "invalid point range clamps to minimum")
            && require(nearVec3(frame.point[0].color, {0.0f, 0.5f, 0.0f}),
                       "invalid point color channels clamp deterministically")
            && require(frame.spot[0].innerConeCos > frame.spot[0].outerConeCos,
                       "invalid spot cones are sanitized to ordered cosine thresholds")
            && require(frame.diagnostics.sanitizedLights >= 2,
                       "lighting diagnostics count sanitized descriptors");
    }

    bool attenuationHelpersAreFiniteAndBounded()
    {
        const float nearOrigin = gts::rendering::pointLightAttenuation(0.0f, 5.0f);
        const float inside = gts::rendering::pointLightAttenuation(2.5f, 5.0f);
        const float nearEdge = gts::rendering::pointLightAttenuation(4.8f, 5.0f);
        const float outside = gts::rendering::pointLightAttenuation(5.1f, 5.0f);
        const float insideCone = gts::rendering::spotConeAttenuation(0.95f, 0.9f, 0.7f);
        const float betweenCone = gts::rendering::spotConeAttenuation(0.8f, 0.9f, 0.7f);
        const float outsideCone = gts::rendering::spotConeAttenuation(0.6f, 0.9f, 0.7f);

        return require(std::isfinite(nearOrigin) && nearOrigin > 0.0f,
                       "point attenuation is finite at the light origin")
            && require(inside > 0.0f, "point attenuation is positive inside range")
            && require(nearEdge > 0.0f && nearEdge < inside,
                       "point attenuation smoothly falls near the range boundary")
            && require(near(outside, 0.0f), "point attenuation is zero outside range")
            && require(near(insideCone, 1.0f), "spot attenuation is full inside inner cone")
            && require(betweenCone > 0.0f && betweenCone < 1.0f,
                       "spot attenuation falls between cone thresholds")
            && require(near(outsideCone, 0.0f), "spot attenuation is zero outside outer cone");
    }

    bool litMaterialVariantAndParametersAreSeparated()
    {
        gts::rendering::MaterialRuntime materials;
        FakeResourceProvider resources;

        MaterialDefinition unlitDefinition;
        unlitDefinition.shaderFamily = MaterialShaderFamily::LegacyUnlit;
        const MaterialDefinitionHandle unlitDef = materials.createDefinition(unlitDefinition);

        MaterialDefinition litDefinition;
        litDefinition.shaderFamily = MaterialShaderFamily::StandardSurface;
        const MaterialDefinitionHandle litDef = materials.createDefinition(litDefinition);

        const MaterialInstanceHandle unlit = materials.createInstance(makeInstance(unlitDef));
        const MaterialInstanceHandle lit = materials.createInstance(makeInstance(litDef));
        const MaterialSyncResult unlitSync = materials.synchronizeGpuState(unlit, &resources);
        const MaterialSyncResult litSync = materials.synchronizeGpuState(lit, &resources);
        const MaterialVariantKey litVariant = litSync.state ? litSync.state->variantKey : MaterialVariantKey{};
        const MaterialVariantKey unlitVariant =
            unlitSync.state ? unlitSync.state->variantKey : MaterialVariantKey{};
        const MaterialShaderFamily unlitFamily = unlitSync.state
            ? unlitSync.state->shaderFamily
            : MaterialShaderFamily::StandardSurface;
        const MaterialShaderFamily litFamily = litSync.state
            ? litSync.state->shaderFamily
            : MaterialShaderFamily::LegacyUnlit;

        materials.modifyInstance(
            lit,
            [](MaterialInstance& instance)
            {
                instance.metallic = 1.4f;
                instance.roughness = 0.0f;
                return true;
            });
        const MaterialSyncResult parameterSync = materials.synchronizeGpuState(lit, &resources);
        const bool parameterOnlyChange =
            parameterSync.changed && parameterSync.parameterChanged && !parameterSync.topologyChanged;
        const MaterialVariantKey parameterVariant =
            parameterSync.state ? parameterSync.state->variantKey : MaterialVariantKey{};
        const bool sanitizedParameters =
            parameterSync.state != nullptr
            && near(parameterSync.state->parameters.metallic(), 1.0f)
            && near(parameterSync.state->parameters.roughness(), MaterialMinimumRoughness);

        materials.modifyInstance(
            lit,
            [unlitDef](MaterialInstance& instance)
            {
                instance.definition = unlitDef;
                return true;
            });
        const MaterialSyncResult topologySync = materials.synchronizeGpuState(lit, &resources);

        return require(unlitSync.state != nullptr && litSync.state != nullptr, "material sync creates states")
            && require(unlitFamily == MaterialShaderFamily::LegacyUnlit,
                       "unlit material keeps LegacyUnlit shader family")
            && require(litFamily == MaterialShaderFamily::StandardSurface,
                       "lit material keeps StandardSurface shader family")
            && require(unlitVariant != litVariant,
                       "lit and unlit material variants differ")
            && require(parameterOnlyChange,
                       "metallic and roughness are parameter-only changes")
            && require(parameterSync.state != nullptr && parameterVariant == litVariant,
                       "metallic and roughness preserve variant key")
            && require(sanitizedParameters,
                       "metallic and roughness are clamped to stable PBR ranges")
            && require(topologySync.changed && topologySync.topologyChanged,
                       "shader-family change is topology-changing")
            && require(topologySync.state != nullptr
                       && topologySync.state->shaderFamily == MaterialShaderFamily::LegacyUnlit,
                       "shader-family mutation updates GPU material family");
    }

    bool normalTransformationHandlesScaleAndReflection()
    {
        constexpr float HalfPi = 1.57079632679f;
        const glm::mat4 rotation =
            glm::rotate(glm::mat4(1.0f), HalfPi, glm::vec3(0.0f, 0.0f, 1.0f));
        const glm::vec3 rotated =
            gts::rendering::transformNormalToWorld(rotation, {1.0f, 0.0f, 0.0f});

        const glm::mat4 nonUniformScale =
            glm::scale(glm::mat4(1.0f), glm::vec3(2.0f, 0.5f, 3.0f));
        const glm::vec3 scaled =
            gts::rendering::transformNormalToWorld(nonUniformScale, {1.0f, 1.0f, 0.0f});
        const glm::vec3 expectedScaled = glm::normalize(glm::vec3(0.5f, 2.0f, 0.0f));

        const glm::mat4 negativeScale =
            glm::scale(glm::mat4(1.0f), glm::vec3(-1.0f, 1.0f, 1.0f));
        const glm::vec3 reflected =
            gts::rendering::transformNormalToWorld(negativeScale, {1.0f, 0.0f, 0.0f});

        return require(nearVec3(rotated, {0.0f, 1.0f, 0.0f}), "normal matrix handles rotation")
            && require(nearVec3(scaled, expectedScaled), "normal matrix handles non-uniform scale")
            && require(nearVec3(reflected, {-1.0f, 0.0f, 0.0f}), "normal matrix handles negative scale")
            && require(finiteVec3(rotated) && finiteVec3(scaled) && finiteVec3(reflected),
                       "normal transformation produces finite normalized output");
    }

    bool pbrHelpersProduceFinitePhysicallyBoundedTerms()
    {
        const glm::vec3 dielectricF0 = gts::rendering::pbrF0ForBaseColor({0.8f, 0.2f, 0.1f}, 0.0f);
        const glm::vec3 metallicF0 = gts::rendering::pbrF0ForBaseColor({0.8f, 0.2f, 0.1f}, 1.0f);
        const glm::vec3 facingFresnel =
            gts::rendering::fresnelSchlick(1.0f, gts::rendering::pbrDielectricF0());
        const glm::vec3 grazingFresnel =
            gts::rendering::fresnelSchlick(0.0f, gts::rendering::pbrDielectricF0());
        const float ggx = gts::rendering::distributionGGX(1.0f, 0.04f);
        const float smith = gts::rendering::geometrySmith(1.0f, 1.0f, 0.5f);

        return require(nearVec3(dielectricF0, {0.04f, 0.04f, 0.04f}),
                       "dielectric F0 uses the expected baseline")
            && require(nearVec3(metallicF0, {0.8f, 0.2f, 0.1f}),
                       "metallic F0 approaches base color")
            && require(grazingFresnel.x > facingFresnel.x &&
                       grazingFresnel.y > facingFresnel.y &&
                       grazingFresnel.z > facingFresnel.z,
                       "Schlick Fresnel increases toward grazing angles")
            && require(std::isfinite(ggx) && ggx >= 0.0f,
                       "GGX distribution remains finite and nonnegative")
            && require(std::isfinite(smith) && smith >= 0.0f && smith <= 1.0f,
                       "Smith geometry term remains finite and bounded");
    }

    bool pbrDirectLightingHandlesAmbientAndBackfaces()
    {
        gts::rendering::DirectionalLightFrameData light;
        light.active = true;
        light.directionToLight = {0.0f, 0.0f, 1.0f};
        light.color = {1.0f, 1.0f, 1.0f};
        light.intensity = 1.0f;

        const glm::vec4 dielectricLit = gts::rendering::evaluateMetallicRoughnessDirectLighting(
            {1.0f, 0.5f, 0.25f, 1.0f},
            {1.0f, 1.0f, 1.0f, 1.0f},
            {0.0f, 0.0f, 1.0f},
            {0.0f, 0.0f, 0.0f},
            {0.0f, 0.0f, 1.0f},
            light,
            0.0f,
            0.0f,
            0.5f);

        const glm::vec4 backFacing = gts::rendering::evaluateMetallicRoughnessDirectLighting(
            {1.0f, 0.5f, 0.25f, 1.0f},
            {1.0f, 1.0f, 1.0f, 1.0f},
            {0.0f, 0.0f, -1.0f},
            {0.0f, 0.0f, 0.0f},
            {0.0f, 0.0f, 1.0f},
            light,
            0.0f,
            0.0f,
            0.5f);

        light.intensity = 0.0f;
        const glm::vec4 dielectricAmbient = gts::rendering::evaluateMetallicRoughnessDirectLighting(
            {0.5f, 0.25f, 0.0f, 0.75f},
            {1.0f, 1.0f, 1.0f, 1.0f},
            {0.0f, 0.0f, 1.0f},
            {0.0f, 0.0f, 0.0f},
            {0.0f, 0.0f, 1.0f},
            light,
            0.2f,
            0.0f,
            0.5f);
        const glm::vec4 metallicAmbient = gts::rendering::evaluateMetallicRoughnessDirectLighting(
            {0.5f, 0.25f, 0.0f, 0.75f},
            {1.0f, 1.0f, 1.0f, 1.0f},
            {0.0f, 0.0f, 1.0f},
            {0.0f, 0.0f, 0.0f},
            {0.0f, 0.0f, 1.0f},
            light,
            0.2f,
            1.0f,
            0.5f);

        const bool finiteLit =
            std::isfinite(dielectricLit.x) && std::isfinite(dielectricLit.y) &&
            std::isfinite(dielectricLit.z) && std::isfinite(dielectricLit.w);

        return require(finiteLit && dielectricLit.x >= 0.0f &&
                       dielectricLit.y >= 0.0f && dielectricLit.z >= 0.0f,
                       "PBR direct lighting produces finite nonnegative output")
            && require(nearVec4(backFacing, {0.0f, 0.0f, 0.0f, 1.0f}),
                       "back-facing light contributes no direct lighting")
            && require(nearVec4(dielectricAmbient, {0.1f, 0.05f, 0.0f, 0.75f}),
                       "dielectric ambient fallback uses base color")
            && require(nearVec4(metallicAmbient, {0.0f, 0.0f, 0.0f, 0.75f}),
                       "metallic surfaces do not receive diffuse ambient fallback");
    }

    bool pbrValidationGridFixtureProducesFiniteSamples()
    {
        gts::rendering::DirectionalLightFrameData light;
        light.active = true;
        light.directionToLight = glm::normalize(glm::vec3(0.25f, 0.4f, 1.0f));
        light.color = {1.0f, 0.96f, 0.9f};
        light.intensity = 2.0f;

        const float metallicValues[] = {0.0f, 0.5f, 1.0f};
        const float roughnessValues[] = {MaterialMinimumRoughness, 0.5f, 1.0f};
        for (float metallic : metallicValues)
        {
            for (float roughness : roughnessValues)
            {
                const glm::vec4 sample = gts::rendering::evaluateMetallicRoughnessDirectLighting(
                    {0.8f, 0.3f, 0.1f, 1.0f},
                    {1.0f, 1.0f, 1.0f, 1.0f},
                    {0.0f, 0.0f, 1.0f},
                    {0.0f, 0.0f, 0.0f},
                    {0.0f, 0.0f, 2.0f},
                    light,
                    0.05f,
                    metallic,
                    roughness);
                if (!std::isfinite(sample.x) || !std::isfinite(sample.y) ||
                    !std::isfinite(sample.z) || !std::isfinite(sample.w))
                {
                    return require(false, "PBR validation grid sample is finite");
                }
            }
        }

        return true;
    }

    bool textureColorSpaceClassificationIsExplicit()
    {
        gts::rendering::MaterialRuntime materials;
        FakeResourceProvider resources;

        const MaterialInstance* defaultInstance = materials.getInstance(materials.defaultMaterial());
        MaterialInstance instance = makeInstance(defaultInstance != nullptr
            ? defaultInstance->definition
            : MaterialDefinitionHandle{});
        instance.baseColorTexture = MaterialTextureBinding::assetPath("textures/base.png");
        const MaterialInstanceHandle baseColor = materials.createInstance(instance);
        materials.synchronizeGpuState(baseColor, &resources);

        instance.baseColorTexture = MaterialTextureBinding::dataAssetPath("textures/roughness.png");
        const MaterialInstanceHandle dataTexture = materials.createInstance(instance);
        materials.synchronizeGpuState(dataTexture, &resources);

        return require(resources.textureColorSpaces["textures/base.png"] == TextureColorSpace::SRgb,
                       "base-color texture requests are classified as sRGB")
            && require(resources.textureColorSpaces["textures/roughness.png"] == TextureColorSpace::Linear,
                       "data texture requests are classified as linear");
    }

    bool extractionPublishesCameraPositionAndLightFrameData()
    {
        ECSWorld world;
        gts::transform::installTransformFeature(world);

        Entity camera = createTransformEntity(world, {2.0f, 3.0f, 4.0f});
        CameraGpuComponent cameraGpu;
        cameraGpu.viewID = 7;
        cameraGpu.active = true;
        cameraGpu.viewMatrix = glm::translate(glm::mat4(1.0f), {-2.0f, -3.0f, -4.0f});
        cameraGpu.projMatrix = glm::mat4(1.0f);
        world.addComponent(camera, cameraGpu);

        Entity lightEntity = createTransformEntity(world);
        DirectionalLightComponent light;
        light.active = true;
        light.intensity = 4.0f;
        light.ambientIntensity = 0.25f;
        world.addComponent(lightEntity, light);

        resolveTransforms(world);

        RenderExtractionSnapshotBuilder builder;
        RenderExtractionSnapshot& snapshot = builder.build(world);

        const bool hasUpload = !snapshot.cameraUploads.empty();
        const CameraUploadCommand upload = hasUpload ? snapshot.cameraUploads.front() : CameraUploadCommand{};

        return require(hasUpload, "camera upload is produced")
            && require(upload.cameraViewID == 7, "camera upload keeps active camera view ID")
            && require(nearVec3(snapshot.cameraWorldPosition, {2.0f, 3.0f, 4.0f}),
                       "snapshot publishes active camera world position")
            && require(nearVec3(upload.cameraWorldPosition, {2.0f, 3.0f, 4.0f}),
                       "camera upload carries camera world position")
            && require(upload.lighting.directionalCount == 1,
                       "camera upload carries selected generic lighting frame")
            && require(near(upload.lighting.directional[0].intensity, 4.0f),
                       "camera upload carries light intensity")
            && require(near(upload.lighting.ambientIntensity, 0.25f),
                       "camera upload carries ambient intensity");
    }

    bool lightingChangesRemainFrameLevelState()
    {
        ECSWorld world;
        gts::transform::installTransformFeature(world);

        auto& materials = gts::rendering::materialRuntime(world);
        MaterialDefinition definition;
        definition.shaderFamily = MaterialShaderFamily::StandardSurface;
        const MaterialDefinitionHandle definitionHandle = materials.createDefinition(definition);
        const MaterialInstanceHandle material = materials.createInstance(makeInstance(definitionHandle));
        const uint64_t versionBefore = materials.getInstance(material)->version;
        const MaterialVariantKey variantBefore =
            makeMaterialVariantKey(MaterialShaderFamily::StandardSurface, *materials.getInstance(material));

        Entity point = createTransformEntity(world);
        PointLightComponent light;
        light.intensity = 2.0f;
        world.addComponent(point, light);
        resolveTransforms(world);
        gts::rendering::extractLightingFrameData(world, {0.0f, 0.0f, 0.0f});

        world.getComponent<PointLightComponent>(point).intensity = 8.0f;
        gts::rendering::extractLightingFrameData(world, {0.0f, 0.0f, 0.0f});

        const MaterialInstance* instance = materials.getInstance(material);
        return require(instance != nullptr, "material instance remains valid after light changes")
            && require(instance->version == versionBefore,
                       "light changes do not mutate material versions")
            && require(makeMaterialVariantKey(MaterialShaderFamily::StandardSurface, *instance) == variantBefore,
                       "light changes do not alter material variant identity");
    }

    bool cameraUboPackingMatchesShaderCapacities()
    {
        static_assert(std::is_standard_layout_v<CameraUBO>);
        static_assert(std::is_standard_layout_v<GpuDirectionalLightData>);
        static_assert(std::is_standard_layout_v<GpuPointLightData>);
        static_assert(std::is_standard_layout_v<GpuSpotLightData>);

        return require(sizeof(GpuDirectionalLightData) == sizeof(glm::vec4) * 2,
                       "directional GPU light packing is two vec4s")
            && require(sizeof(GpuPointLightData) == sizeof(glm::vec4) * 2,
                       "point GPU light packing is two vec4s")
            && require(sizeof(GpuSpotLightData) == sizeof(glm::vec4) * 4,
                       "spot GPU light packing is four vec4s")
            && require(offsetof(CameraUBO, directional) > offsetof(CameraUBO, lightingCountsAmbient),
                       "directional light array follows lighting counts")
            && require(offsetof(CameraUBO, point) > offsetof(CameraUBO, directional),
                       "point light array follows directional lights")
            && require(offsetof(CameraUBO, spot) > offsetof(CameraUBO, point),
                       "spot light array follows point lights")
            && require(sizeof(CameraUBO) < 16u * 1024u,
                       "bounded lighting UBO stays comfortably below common uniform limits");
    }
}

int main()
{
    bool ok = true;
    ok = directionalLightSelectionIsPriorityBounded() && ok;
    ok = noActiveLightFallsBackToAmbientOnly() && ok;
    ok = parentedLightUsesResolvedWorldTransform() && ok;
    ok = lightMutationUpdatesExtractedFrameData() && ok;
    ok = pointAndSpotLightsUseResolvedWorldTransforms() && ok;
    ok = localLightSelectionIsPriorityDistanceAndCapacityBounded() && ok;
    ok = invalidLightValuesAreSanitized() && ok;
    ok = attenuationHelpersAreFiniteAndBounded() && ok;
    ok = litMaterialVariantAndParametersAreSeparated() && ok;
    ok = normalTransformationHandlesScaleAndReflection() && ok;
    ok = pbrHelpersProduceFinitePhysicallyBoundedTerms() && ok;
    ok = pbrDirectLightingHandlesAmbientAndBackfaces() && ok;
    ok = pbrValidationGridFixtureProducesFiniteSamples() && ok;
    ok = textureColorSpaceClassificationIsExplicit() && ok;
    ok = extractionPublishesCameraPositionAndLightFrameData() && ok;
    ok = lightingChangesRemainFrameLevelState() && ok;
    ok = cameraUboPackingMatchesShaderCapacities() && ok;

    if (!ok)
        return 1;

    std::printf("First lit renderer architecture tests passed\n");
    return 0;
}
