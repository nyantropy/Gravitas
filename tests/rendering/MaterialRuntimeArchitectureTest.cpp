#include <cstdio>
#include <string>
#include <unordered_map>
#include <vector>

#include "BitmapFont.h"
#include "BoundsComponent.h"
#include "ECSWorld.hpp"
#include "EcsControllerContext.hpp"
#include "IResourceProvider.hpp"
#include "MaterialReferenceComponent.h"
#include "MaterialRuntime.h"
#include "MeshGpuComponent.h"
#include "RenderDirtyComponent.h"
#include "RenderExtractionSnapshotBuilder.hpp"
#include "RenderGpuComponent.h"
#include "RendererGeometrySceneFeature.h"
#include "StaticMeshComponent.h"
#include "TextureAnimationComponent.h"
#include "TimeContext.h"
#include "TransformComponent.h"
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

    struct FakeResourceProvider : IResourceProvider
    {
        mesh_id_type nextMesh = 100;
        texture_id_type nextTexture = 200;
        ssbo_id_type nextSlot = 0;
        font_id_type nextFont = 300;

        uint32_t meshRequests = 0;
        uint32_t textureRequests = 0;
        uint32_t objectSlotRequests = 0;

        std::unordered_map<std::string, mesh_id_type> meshes;
        std::unordered_map<std::string, texture_id_type> textures;

        mesh_id_type requestMesh(const std::string& path) override
        {
            ++meshRequests;
            auto [it, inserted] = meshes.emplace(path, nextMesh);
            if (inserted)
                ++nextMesh;
            return it->second;
        }

        mesh_id_type getSharedQuadMesh(float, float) override
        {
            return nextMesh++;
        }

        mesh_id_type uploadProceduralMesh(mesh_id_type existingId,
                                          const std::vector<Vertex>&,
                                          const std::vector<uint32_t>&) override
        {
            if (existingId != 0)
                return existingId;
            return nextMesh++;
        }

        void releaseProceduralMesh(mesh_id_type) override
        {
        }

        texture_id_type requestTexture(const std::string& path) override
        {
            ++textureRequests;
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
            return nextFont++;
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
            ++objectSlotRequests;
            return nextSlot++;
        }

        void releaseObjectSlot(ssbo_id_type) override
        {
        }
    };

    void install(ECSWorld& world, FakeResourceProvider& resources)
    {
        gts::transform::installTransformFeature(world);
        gts::rendering::installRendererGeometrySceneFeature(world, &resources);
    }

    void update(ECSWorld& world, FakeResourceProvider& resources, const TimeContext* time = nullptr)
    {
        EcsControllerContext ctx{world};
        ctx.resources = &resources;
        ctx.time = time;
        world.updateControllers(ctx);
    }

    MaterialInstance makeTexturedInstance(gts::rendering::MaterialRuntime& materials,
                                          const std::string& texturePath,
                                          const glm::vec4& color)
    {
        MaterialInstance instance;
        const MaterialInstance* defaultInstance = materials.getInstance(materials.defaultMaterial());
        if (defaultInstance != nullptr)
            instance.definition = defaultInstance->definition;
        instance.baseColor = color;
        instance.baseColorTexture = MaterialTextureBinding::assetPath(texturePath);
        instance.renderState.alphaMode =
            alphaModeForLegacyMaterial(MaterialBlendMode::Alpha, color.a, true);
        return instance;
    }

    Entity createStaticRenderable(ECSWorld& world,
                                  const std::string& meshPath,
                                  MaterialInstanceHandle material)
    {
        Entity entity = world.createEntity();
        TransformComponent transform;
        world.addComponent(entity, transform);

        StaticMeshComponent mesh;
        mesh.meshPath = meshPath;
        world.addComponent(entity, mesh);
        world.addComponent(entity, MaterialReferenceComponent{material});
        world.addComponent(entity, BoundsComponent{});
        return entity;
    }

    bool handlesVersionsAndClone()
    {
        gts::rendering::MaterialRuntime materials;
        FakeResourceProvider resources;

        MaterialDefinition definition;
        definition.shaderFamily = MaterialShaderFamily::LegacyUnlit;
        const MaterialDefinitionHandle definitionHandle = materials.createDefinition(definition);
        MaterialInstance instance = makeTexturedInstance(materials, "textures/shared.png", {1.0f, 1.0f, 1.0f, 1.0f});
        instance.definition = definitionHandle;

        const MaterialInstanceHandle handle = materials.createInstance(instance);
        const MaterialSyncResult firstSync = materials.synchronizeGpuState(handle, &resources);
        const bool firstSyncResolved =
            firstSync.changed && firstSync.state != nullptr && firstSync.state->baseColorTextureID != 0;
        const uint64_t firstVersion = materials.getInstance(handle)->version;

        const bool modified = materials.modifyInstance(
            handle,
            [](MaterialInstance& editable)
            {
                editable.baseColor = {0.25f, 0.5f, 0.75f, 1.0f};
                return true;
            });
        const MaterialSyncResult secondSync = materials.synchronizeGpuState(handle, &resources);
        const bool secondSyncUpdatedParameters = secondSync.changed && secondSync.parameterChanged;
        const uint64_t updatedVersion = materials.getInstance(handle)->version;

        const MaterialInstanceHandle clone = materials.cloneInstance(handle);
        materials.modifyInstance(
            clone,
            [](MaterialInstance& editable)
            {
                editable.baseColor = {1.0f, 0.0f, 0.0f, 1.0f};
                return true;
            });
        const bool cloneIndependent =
            materials.getInstance(clone)->baseColor != materials.getInstance(handle)->baseColor;

        const bool destroyed = materials.destroyInstance(handle);

        return require(definitionHandle.valid(), "material definition handle is valid")
            && require(handle.valid(), "material instance handle is valid")
            && require(firstSync.changed, "first material sync creates GPU cache state")
            && require(firstSyncResolved, "material GPU cache resolves texture")
            && require(modified, "material instance mutation reports change")
            && require(updatedVersion == firstVersion + 1, "material mutation increments version")
            && require(secondSyncUpdatedParameters, "material parameter change synchronizes GPU state")
            && require(resources.textureRequests == 1, "base-color-only update does not request texture again")
            && require(clone.valid(), "clone creates valid independent material handle")
            && require(cloneIndependent, "clone mutation does not mutate source instance")
            && require(destroyed, "scene material instance can be destroyed")
            && require(!materials.isInstanceAlive(handle), "destroyed material handle is no longer alive")
            && require(materials.resolveInstanceHandle(handle) == materials.defaultMaterial(),
                       "destroyed material reference resolves to default fallback");
    }

    bool sharedMaterialUpdatesAllReferencingRenderables()
    {
        ECSWorld world;
        FakeResourceProvider resources;
        install(world, resources);

        auto& materials = gts::rendering::materialRuntime(world);
        const MaterialInstanceHandle shared = materials.createInstance(
            makeTexturedInstance(materials, "textures/shared.png", {1.0f, 1.0f, 1.0f, 1.0f}));
        const Entity a = createStaticRenderable(world, "mesh/a.obj", shared);
        const Entity b = createStaticRenderable(world, "mesh/b.obj", shared);
        update(world, resources);

        const mesh_id_type meshA = world.getComponent<MeshGpuComponent>(a).meshID;
        const mesh_id_type meshB = world.getComponent<MeshGpuComponent>(b).meshID;
        const uint32_t meshRequests = resources.meshRequests;
        const MaterialGpuState* state = materials.getGpuState(shared);
        const uint64_t uploadedVersion = state ? state->uploadedVersion : 0;

        materials.modifyInstance(
            shared,
            [](MaterialInstance& editable)
            {
                editable.baseColor = {0.2f, 0.4f, 0.8f, 1.0f};
                return true;
            });
        update(world, resources);

        const bool aDirty = world.hasComponent<RenderDirtyComponent>(a)
            && world.getComponent<RenderDirtyComponent>(a).materialDirty
            && world.getComponent<RenderDirtyComponent>(a).objectDataDirty;
        const bool bDirty = world.hasComponent<RenderDirtyComponent>(b)
            && world.getComponent<RenderDirtyComponent>(b).materialDirty
            && world.getComponent<RenderDirtyComponent>(b).objectDataDirty;
        const MaterialGpuState* updatedState = materials.getGpuState(shared);

        return require(world.getComponent<MaterialReferenceComponent>(a).material == shared,
                       "first renderable references shared material")
            && require(world.getComponent<MaterialReferenceComponent>(b).material == shared,
                       "second renderable references shared material")
            && require(world.hasComponent<RenderGpuComponent>(a) && world.hasComponent<RenderGpuComponent>(b),
                       "both shared-material renderables become render objects")
            && require(resources.textureRequests == 1, "shared material resolves one texture cache entry")
            && require(materials.gpuStateCount() == 1, "shared material creates one GPU material cache state")
            && require(updatedState != nullptr && updatedState->uploadedVersion == uploadedVersion + 1,
                       "shared material update uploads one material cache version")
            && require(aDirty && bDirty, "shared material update dirties every referencing renderable")
            && require(world.getComponent<MeshGpuComponent>(a).meshID == meshA &&
                       world.getComponent<MeshGpuComponent>(b).meshID == meshB,
                       "shared material update preserves mesh GPU state")
            && require(resources.meshRequests == meshRequests, "shared material update does not request meshes");
    }

    bool extractionCarriesStableMaterialIdentity()
    {
        ECSWorld world;
        FakeResourceProvider resources;
        install(world, resources);

        auto& materials = gts::rendering::materialRuntime(world);
        const MaterialInstanceHandle shared = materials.createInstance(
            makeTexturedInstance(materials, "textures/shared.png", {0.8f, 0.9f, 1.0f, 1.0f}));
        createStaticRenderable(world, "mesh/a.obj", shared);
        createStaticRenderable(world, "mesh/b.obj", shared);
        update(world, resources);

        RenderExtractionSnapshotBuilder builder;
        RenderExtractionSnapshot& snapshot = builder.build(world);

        bool allCarrySharedMaterial = snapshot.renderables.size() == 2;
        for (const RenderableSnapshot& renderable : snapshot.renderables)
        {
            allCarrySharedMaterial =
                allCarrySharedMaterial
                && renderable.material == shared
                && renderable.materialGpu.valid()
                && renderable.textureID != 0;
        }

        return require(allCarrySharedMaterial,
                       "render extraction carries material instance and GPU cache handles");
    }

    bool perObjectUvAnimationDoesNotMutateSharedMaterial()
    {
        ECSWorld world;
        FakeResourceProvider resources;
        install(world, resources);

        auto& materials = gts::rendering::materialRuntime(world);
        const MaterialInstanceHandle shared = materials.createInstance(
            makeTexturedInstance(materials, "textures/shared.png", {1.0f, 1.0f, 1.0f, 1.0f}));
        const Entity animated = createStaticRenderable(world, "mesh/a.obj", shared);
        const Entity staticEntity = createStaticRenderable(world, "mesh/b.obj", shared);
        update(world, resources);

        const uint64_t materialVersion = materials.getGpuState(shared)->uploadedVersion;
        TextureAnimationComponent animation;
        animation.enabled = true;
        animation.scrollSpeed = {1.0f, 0.0f};
        world.addComponent(animated, animation);

        TimeContext time;
        time.unscaledDeltaTime = 0.25f;
        time.deltaTime = 0.25f;
        update(world, resources, &time);

        return require(world.getComponent<RenderGpuComponent>(animated).uvTransform.z !=
                       world.getComponent<RenderGpuComponent>(staticEntity).uvTransform.z,
                       "shared-material renderables can have independent UV animation")
            && require(materials.getGpuState(shared)->uploadedVersion == materialVersion,
                       "per-object UV animation does not mutate shared material state");
    }

    bool destroyedSharedMaterialFallsBackSafely()
    {
        ECSWorld world;
        FakeResourceProvider resources;
        install(world, resources);

        auto& materials = gts::rendering::materialRuntime(world);
        const MaterialInstanceHandle shared = materials.createInstance(
            makeTexturedInstance(materials, "textures/shared.png", {1.0f, 1.0f, 1.0f, 1.0f}));
        const Entity entity = createStaticRenderable(world, "mesh/a.obj", shared);
        update(world, resources);

        materials.destroyInstance(shared);
        update(world, resources);

        return require(world.getComponent<MaterialReferenceComponent>(entity).material == materials.defaultMaterial(),
                       "destroyed material references are replaced with the default material")
            && require(materials.getGpuState(materials.defaultMaterial()) != nullptr,
                       "default fallback material has synchronized GPU state")
            && require(world.hasComponent<RenderGpuComponent>(entity),
                       "fallback material keeps render object valid");
    }
}

int main()
{
    bool ok = true;
    ok &= handlesVersionsAndClone();
    ok &= sharedMaterialUpdatesAllReferencingRenderables();
    ok &= extractionCarriesStableMaterialIdentity();
    ok &= perObjectUvAnimationDoesNotMutateSharedMaterial();
    ok &= destroyedSharedMaterialFallsBackSafely();

    if (!ok)
        return 1;

    std::printf("MaterialRuntimeArchitectureTest passed\n");
    return 0;
}
