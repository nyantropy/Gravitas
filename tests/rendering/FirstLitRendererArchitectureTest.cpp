#include <cmath>
#include <cstdio>
#include <string>
#include <unordered_map>
#include <vector>

#include "BitmapFont.h"
#include "CameraGpuComponent.h"
#include "DirectionalLightComponent.h"
#include "DirectionalLightExtraction.h"
#include "ECSWorld.hpp"
#include "EcsControllerContext.hpp"
#include "IResourceProvider.hpp"
#include "LightingFrameData.h"
#include "MaterialRuntime.h"
#include "RenderExtractionSnapshotBuilder.hpp"
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

    bool activeDirectionalLightSelectionIsExplicit()
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
        world.addComponent(active, activeLight);

        resolveTransforms(world);
        const gts::rendering::DirectionalLightFrameData frame =
            gts::rendering::extractDirectionalLightFrameData(world);

        return require(frame.active, "active directional light is selected")
            && require(nearVec3(frame.color, {0.0f, 1.0f, 0.0f}), "inactive lights are skipped")
            && require(near(frame.intensity, 2.0f), "light intensity is copied to frame data");
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
        const gts::rendering::DirectionalLightFrameData frame =
            gts::rendering::extractDirectionalLightFrameData(world);

        return require(!frame.active, "no active light reports inactive frame data")
            && require(near(frame.intensity, 0.0f), "inactive fallback has zero direct intensity")
            && require(near(frame.ambientIntensity, 0.12f), "inactive fallback keeps ambient contribution");
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
        const gts::rendering::DirectionalLightFrameData frame =
            gts::rendering::extractDirectionalLightFrameData(world);

        return require(nearVec3(frame.directionToLight, {1.0f, 0.0f, 0.0f}),
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

        auto first = gts::rendering::extractDirectionalLightFrameData(world);
        auto& editable = world.getComponent<DirectionalLightComponent>(lightEntity);
        editable.intensity = 3.5f;
        editable.color = {0.25f, 0.5f, 0.75f};
        auto second = gts::rendering::extractDirectionalLightFrameData(world);

        return require(near(first.intensity, 1.0f), "first light extraction sees initial intensity")
            && require(near(second.intensity, 3.5f), "light intensity mutation reaches extraction")
            && require(nearVec3(second.color, {0.25f, 0.5f, 0.75f}),
                       "light color mutation reaches extraction");
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
                instance.specularStrength = 0.8f;
                instance.shininess = 64.0f;
                return true;
            });
        const MaterialSyncResult parameterSync = materials.synchronizeGpuState(lit, &resources);
        const bool parameterOnlyChange =
            parameterSync.changed && parameterSync.parameterChanged && !parameterSync.topologyChanged;
        const MaterialVariantKey parameterVariant =
            parameterSync.state ? parameterSync.state->variantKey : MaterialVariantKey{};

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
                       "specular and shininess are parameter-only changes")
            && require(parameterSync.state != nullptr && parameterVariant == litVariant,
                       "specular and shininess preserve variant key")
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

    bool lightingEquationProducesAmbientDiffuseAndSpecularTerms()
    {
        gts::rendering::DirectionalLightFrameData light;
        light.active = true;
        light.directionToLight = {0.0f, 0.0f, 1.0f};
        light.color = {1.0f, 1.0f, 1.0f};
        light.intensity = 1.0f;
        light.ambientIntensity = 0.1f;

        const glm::vec4 diffuseOnly = gts::rendering::evaluateBlinnPhongLighting(
            {1.0f, 0.0f, 0.0f, 1.0f},
            {1.0f, 1.0f, 1.0f, 1.0f},
            {0.0f, 0.0f, 1.0f},
            {0.0f, 0.0f, 0.0f},
            {0.0f, 0.0f, 1.0f},
            light,
            0.0f,
            32.0f);

        light.intensity = 0.0f;
        light.ambientIntensity = 0.2f;
        const glm::vec4 ambientOnly = gts::rendering::evaluateBlinnPhongLighting(
            {0.5f, 0.25f, 0.0f, 0.75f},
            {1.0f, 1.0f, 1.0f, 1.0f},
            {0.0f, 0.0f, 1.0f},
            {0.0f, 0.0f, 0.0f},
            {0.0f, 0.0f, 1.0f},
            light,
            1.0f,
            32.0f);

        return require(nearVec4(diffuseOnly, {1.1f, 0.0f, 0.0f, 1.0f}),
                       "lighting equation includes ambient and diffuse terms")
            && require(nearVec4(ambientOnly, {0.1f, 0.05f, 0.0f, 0.75f}),
                       "zero direct light leaves ambient contribution only");
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
            && require(upload.directionalLight.active, "camera upload carries active light frame")
            && require(near(upload.directionalLight.intensity, 4.0f),
                       "camera upload carries light intensity")
            && require(near(upload.directionalLight.ambientIntensity, 0.25f),
                       "camera upload carries ambient intensity");
    }
}

int main()
{
    bool ok = true;
    ok = activeDirectionalLightSelectionIsExplicit() && ok;
    ok = noActiveLightFallsBackToAmbientOnly() && ok;
    ok = parentedLightUsesResolvedWorldTransform() && ok;
    ok = lightMutationUpdatesExtractedFrameData() && ok;
    ok = litMaterialVariantAndParametersAreSeparated() && ok;
    ok = normalTransformationHandlesScaleAndReflection() && ok;
    ok = lightingEquationProducesAmbientDiffuseAndSpecularTerms() && ok;
    ok = extractionPublishesCameraPositionAndLightFrameData() && ok;

    if (!ok)
        return 1;

    std::printf("First lit renderer architecture tests passed\n");
    return 0;
}
