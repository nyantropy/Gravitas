#include <cstdio>
#include <cmath>
#include <limits>
#include <string>
#include <unordered_map>
#include <vector>

#include "BitmapFont.h"
#include "BoundsComponent.h"
#include "ECSWorld.hpp"
#include "EcsControllerContext.hpp"
#include "IResourceProvider.hpp"
#include "MaterialBindingSystem.hpp"
#include "MaterialComponent.h"
#include "MaterialReferenceComponent.h"
#include "MaterialRuntime.h"
#include "MeshGpuComponent.h"
#include "RenderDirtyComponent.h"
#include "RenderCommandExtractor.hpp"
#include "RenderExtractionSnapshotBuilder.hpp"
#include "RenderGpuComponent.h"
#include "RendererGeometrySceneFeature.h"
#include "StaticMeshComponent.h"
#include "TextureAnimationComponent.h"
#include "TimeContext.h"
#include "TransformComponent.h"
#include "TransformSceneFeature.h"
#include "Vertex.h"
#include "WorldTextComponent.h"

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
        std::unordered_map<std::string, TextureColorSpace> textureColorSpaces;
        std::unordered_map<std::string, font_id_type> fontsByPath;
        std::unordered_map<font_id_type, BitmapFont> fonts;

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
                                          const std::vector<uint32_t>&,
                                          VertexAttributeFlags = LegacyUnlitVertexAttributes) override
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
            return requestTexture(path, TextureColorSpace::SRgb);
        }

        texture_id_type requestTexture(const std::string& path, TextureColorSpace colorSpace) override
        {
            ++textureRequests;
            const std::string key = path
                + (colorSpace == TextureColorSpace::SRgb ? ":srgb" : ":linear");
            textureColorSpaces[path] = colorSpace;
            auto [it, inserted] = textures.emplace(key, nextTexture);
            if (inserted)
                ++nextTexture;
            return it->second;
        }

        texture_id_type requestMaterialFallbackTexture(MaterialTextureRole role) override
        {
            const std::string path = std::string("__fallback/") + fallbackTextureNameForRole(role);
            return requestTexture(path, textureColorSpaceForRole(role));
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

        font_id_type requestFont(const std::string& path) override
        {
            auto [it, inserted] = fontsByPath.emplace(path, nextFont);
            if (inserted)
            {
                BitmapFont font;
                font.atlasTexture = 900 + nextFont;
                font.lineHeight = 10.0f;
                font.glyphs['A'] = GlyphInfo{
                    {0.0f, 0.0f},
                    {1.0f, 1.0f},
                    {8.0f, 8.0f},
                    {0.0f, 8.0f},
                    8.0f
                };
                font.glyphs['B'] = font.glyphs['A'];
                fonts.emplace(nextFont, font);
                ++nextFont;
            }
            return it->second;
        }

        const BitmapFont* getFont(font_id_type id) const override
        {
            auto it = fonts.find(id);
            return it == fonts.end() ? nullptr : &it->second;
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
            && require(resources.textureRequests == 5, "base-color-only update does not request texture set again")
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
        RenderExtractionSnapshotBuilder builder;
        builder.build(world);

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

        const bool aClean = world.hasComponent<RenderDirtyComponent>(a)
            && !world.getComponent<RenderDirtyComponent>(a).materialDirty
            && !world.getComponent<RenderDirtyComponent>(a).objectDataDirty;
        const bool bClean = world.hasComponent<RenderDirtyComponent>(b)
            && !world.getComponent<RenderDirtyComponent>(b).materialDirty
            && !world.getComponent<RenderDirtyComponent>(b).objectDataDirty;
        const MaterialGpuState* updatedState = materials.getGpuState(shared);
        const auto materialMetrics = MaterialBindingSystem::getLastMetrics();
        RenderExtractionSnapshot& updatedSnapshot = builder.build(world);
        const MaterialFrameState* updatedFrameState =
            updatedSnapshot.materialFrameData.find(updatedState ? updatedState->gpuHandle : MaterialGpuHandle{});

        return require(world.getComponent<MaterialReferenceComponent>(a).material == shared,
                       "first renderable references shared material")
            && require(world.getComponent<MaterialReferenceComponent>(b).material == shared,
                       "second renderable references shared material")
            && require(world.hasComponent<RenderGpuComponent>(a) && world.hasComponent<RenderGpuComponent>(b),
                       "both shared-material renderables become render objects")
            && require(resources.textureRequests == 5, "shared material resolves one texture set")
            && require(materials.gpuStateCount() == 1, "shared material creates one GPU material cache state")
            && require(updatedState != nullptr && updatedState->uploadedVersion == uploadedVersion + 1,
                       "shared material update uploads one material cache version")
            && require(aClean && bClean, "parameter-only material update keeps renderable dirty flags clean")
            && require(materialMetrics.queuedMaterials == 1 &&
                       materialMetrics.synchronizedMaterials == 1 &&
                       materialMetrics.userInvalidations == 0 &&
                       materialMetrics.fullMaterialScans == 0,
                       "parameter-only material update synchronizes one queued material without user invalidation")
            && require(updatedSnapshot.objectUploads.empty(),
                       "shared material color update does not enqueue object uploads")
            && require(updatedFrameState != nullptr &&
                       near(updatedFrameState->parameters.baseColor.r, 0.2f) &&
                       near(updatedFrameState->parameters.baseColor.g, 0.4f) &&
                       near(updatedFrameState->parameters.baseColor.b, 0.8f),
                       "updated shared base color is carried by material frame state")
            && require(world.getComponent<MeshGpuComponent>(a).meshID == meshA &&
                       world.getComponent<MeshGpuComponent>(b).meshID == meshB,
                       "shared material update preserves mesh GPU state")
            && require(resources.meshRequests == meshRequests, "shared material update does not request meshes");
    }

    bool materialUserIndexTracksAddRemoveReplaceAndReset()
    {
        ECSWorld world;
        auto& materials = gts::rendering::materialRuntime(world);
        const MaterialInstanceHandle first = materials.createInstance(MaterialInstance{});
        const MaterialInstanceHandle second = materials.createInstance(MaterialInstance{});
        const Entity entity = world.createEntity();

        gts::rendering::registerMaterialUser(world, entity, first);
        gts::rendering::registerMaterialUser(world, entity, first);
        const bool duplicatePrevented =
            gts::rendering::materialUserCount(world, first) == 1
            && gts::rendering::indexedMaterialForEntity(world, entity) == first;

        gts::rendering::registerMaterialUser(world, entity, second);
        const bool replaced =
            gts::rendering::materialUserCount(world, first) == 0
            && gts::rendering::materialUserCount(world, second) == 1
            && gts::rendering::indexedMaterialForEntity(world, entity) == second;

        gts::rendering::unregisterMaterialUser(world, entity);
        const bool removed =
            gts::rendering::materialUserCount(world, second) == 0
            && !gts::rendering::indexedMaterialForEntity(world, entity).valid();

        gts::rendering::registerMaterialUser(world, entity, first);
        gts::rendering::resetGeometryBindingLifecycleState(world);
        const bool reset =
            gts::rendering::materialUserCount(world, first) == 0
            && !gts::rendering::indexedMaterialForEntity(world, entity).valid();

        return require(duplicatePrevented, "material user index prevents duplicate users")
            && require(replaced, "material user index replaces entity material ownership")
            && require(removed, "material user index removes entity ownership")
            && require(reset, "material user index resets with scene lifecycle state");
    }

    bool materialReferenceCallbacksMaintainUserIndex()
    {
        ECSWorld world;
        FakeResourceProvider resources;
        install(world, resources);

        auto& materials = gts::rendering::materialRuntime(world);
        const MaterialInstanceHandle material = materials.createInstance(MaterialInstance{});
        const Entity entity = world.createEntity();
        world.addComponent(entity, MaterialReferenceComponent{material});
        const bool added =
            gts::rendering::materialUserCount(world, material) == 1
            && gts::rendering::indexedMaterialForEntity(world, entity) == material;

        world.destroyEntity(entity);
        const bool removed =
            gts::rendering::materialUserCount(world, material) == 0
            && !gts::rendering::indexedMaterialForEntity(world, entity).valid();

        return require(added, "material reference add callback registers user")
            && require(removed, "entity destruction removes material user");
    }

    bool legacyAndWorldTextDescriptorsQueueMaterialSync()
    {
        ECSWorld world;
        FakeResourceProvider resources;
        install(world, resources);

        Entity legacy = world.createEntity();
        StaticMeshComponent mesh;
        mesh.meshPath = "mesh/legacy.obj";
        MaterialComponent material;
        material.texturePath = "textures/legacy.png";
        world.addComponent(legacy, TransformComponent{});
        world.addComponent(legacy, mesh);
        world.addComponent(legacy, material);
        update(world, resources);
        const auto legacyMetrics = MaterialBindingSystem::getLastMetrics();

        update(world, resources);
        const auto steadyMetrics = MaterialBindingSystem::getLastMetrics();

        Entity text = world.createEntity();
        WorldTextComponent worldText;
        worldText.text = "AB";
        worldText.fontPath = "fonts/test.fnt";
        world.addComponent(text, TransformComponent{});
        world.addComponent(text, worldText);
        update(world, resources);
        const auto textMetrics = MaterialBindingSystem::getLastMetrics();

        return require(legacyMetrics.queuedMaterials == 1 &&
                       legacyMetrics.synchronizedMaterials == 1 &&
                       legacyMetrics.fullMaterialScans == 0,
                       "legacy material descriptor queues one material sync without a scan")
            && require(steadyMetrics.queuedMaterials == 0 &&
                       steadyMetrics.synchronizedMaterials == 0 &&
                       steadyMetrics.fullMaterialScans == 0,
                       "legacy material steady state performs no material sync work")
            && require(textMetrics.queuedMaterials == 1 &&
                       textMetrics.synchronizedMaterials == 1 &&
                       textMetrics.fullMaterialScans == 0,
                       "world-text material descriptor queues one material sync without a scan");
    }

    bool steadyStateMaterialBindingDoesNoSynchronizationWork()
    {
        ECSWorld world;
        FakeResourceProvider resources;
        install(world, resources);

        auto& materials = gts::rendering::materialRuntime(world);
        const MaterialInstanceHandle shared = materials.createInstance(
            makeTexturedInstance(materials, "textures/shared.png", {1.0f, 1.0f, 1.0f, 1.0f}));
        createStaticRenderable(world, "mesh/a.obj", shared);
        createStaticRenderable(world, "mesh/b.obj", shared);
        update(world, resources);
        update(world, resources);

        const auto materialMetrics = MaterialBindingSystem::getLastMetrics();
        return require(materialMetrics.queuedMaterials == 0,
                       "steady-state material queue is empty")
            && require(materialMetrics.synchronizedMaterials == 0,
                       "steady-state performs zero material synchronizations")
            && require(materialMetrics.userInvalidations == 0,
                       "steady-state performs zero material user invalidations")
            && require(materialMetrics.fullMaterialScans == 0,
                       "steady-state performs zero full material scans");
    }

    bool topologyChangeInvalidatesOnlyIndexedUsers()
    {
        ECSWorld world;
        FakeResourceProvider resources;
        install(world, resources);

        auto& materials = gts::rendering::materialRuntime(world);
        const MaterialInstanceHandle shared = materials.createInstance(
            makeTexturedInstance(materials, "textures/shared.png", {1.0f, 1.0f, 1.0f, 1.0f}));
        const MaterialInstanceHandle other = materials.createInstance(
            makeTexturedInstance(materials, "textures/other.png", {1.0f, 1.0f, 1.0f, 1.0f}));
        const Entity a = createStaticRenderable(world, "mesh/a.obj", shared);
        const Entity b = createStaticRenderable(world, "mesh/b.obj", shared);
        const Entity c = createStaticRenderable(world, "mesh/c.obj", other);
        update(world, resources);
        RenderExtractionSnapshotBuilder builder;
        builder.build(world);

        materials.modifyInstance(
            shared,
            [](MaterialInstance& editable)
            {
                editable.renderState.doubleSided = !editable.renderState.doubleSided;
                return true;
            });
        update(world, resources);

        const auto materialMetrics = MaterialBindingSystem::getLastMetrics();
        const bool sharedUsersDirty =
            world.getComponent<RenderDirtyComponent>(a).materialDirty
            && world.getComponent<RenderDirtyComponent>(b).materialDirty;
        const bool unrelatedClean =
            !world.getComponent<RenderDirtyComponent>(c).materialDirty
            && !world.getComponent<RenderDirtyComponent>(c).objectDataDirty;

        return require(materialMetrics.queuedMaterials == 1 &&
                       materialMetrics.synchronizedMaterials == 1,
                       "topology change synchronizes one queued shared material")
            && require(materialMetrics.userInvalidations == 2,
                       "topology change invalidates only indexed users")
            && require(sharedUsersDirty, "indexed users are marked material-dirty")
            && require(unrelatedClean, "unrelated material user remains clean");
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
        bool materialFrameStatePresent = snapshot.materialFrameData.materials.size() == 1;
        for (const RenderableSnapshot& renderable : snapshot.renderables)
        {
            const MaterialFrameState* materialState = snapshot.materialFrameData.find(renderable.materialGpu);
            allCarrySharedMaterial =
                allCarrySharedMaterial
                && renderable.material == shared
                && renderable.materialGpu.valid()
                && renderable.variantKey.value != 0
                && renderable.renderQueue == RenderQueue::Opaque
                && materialState != nullptr
                && materialState->baseColorTextureID != 0;
        }

        return require(allCarrySharedMaterial,
                       "render extraction carries material instance and GPU cache handles")
            && require(materialFrameStatePresent,
                       "shared material appears once in material frame data");
    }

    bool renderCommandsUseMaterialIdentity()
    {
        ECSWorld world;
        FakeResourceProvider resources;
        install(world, resources);

        auto& materials = gts::rendering::materialRuntime(world);
        const MaterialInstanceHandle shared = materials.createInstance(
            makeTexturedInstance(materials, "textures/shared.png", {1.0f, 1.0f, 1.0f, 1.0f}));
        createStaticRenderable(world, "mesh/a.obj", shared);
        createStaticRenderable(world, "mesh/b.obj", shared);
        update(world, resources);

        RenderExtractionSnapshotBuilder builder;
        RenderExtractionSnapshot& snapshot = builder.build(world);
        RenderCommandExtractor extractor;
        const std::vector<RenderCommand>& commands = extractor.extract(snapshot);

        bool commandsCarryMaterialIdentity = commands.size() == 2;
        for (const RenderCommand& command : commands)
        {
            commandsCarryMaterialIdentity =
                commandsCarryMaterialIdentity
                && command.material == shared
                && command.materialGpu.valid()
                && command.variantKey.value != 0
                && command.renderQueue == RenderQueue::Opaque
                && snapshot.materialFrameData.find(command.materialGpu) != nullptr;
        }

        return require(commandsCarryMaterialIdentity,
                       "render commands carry material handles, variant key, and queue")
            && require(snapshot.materialFrameData.materials.size() == 1,
                       "shared commands resolve one material frame state")
            && require(snapshot.objectUploads.size() == 2,
                       "object uploads remain per-object model/UV data");
    }

    bool variantKeySeparatesTopologyFromParameters()
    {
        gts::rendering::MaterialRuntime materials;
        FakeResourceProvider resources;

        MaterialInstance instance = makeTexturedInstance(
            materials,
            "textures/shared.png",
            {1.0f, 1.0f, 1.0f, 1.0f});
        const MaterialInstanceHandle handle = materials.createInstance(instance);
        const MaterialSyncResult firstSync = materials.synchronizeGpuState(handle, &resources);
        const MaterialVariantKey initialVariant = firstSync.state->variantKey;

        materials.modifyInstance(
            handle,
            [](MaterialInstance& editable)
            {
                editable.baseColor = {0.25f, 0.5f, 0.75f, 1.0f};
                return true;
            });
        const MaterialSyncResult parameterSync = materials.synchronizeGpuState(handle, &resources);
        const MaterialVariantKey parameterVariant = parameterSync.state->variantKey;

        materials.modifyInstance(
            handle,
            [](MaterialInstance& editable)
            {
                editable.renderState.doubleSided = !editable.renderState.doubleSided;
                return true;
            });
        const MaterialSyncResult topologySync = materials.synchronizeGpuState(handle, &resources);

        materials.modifyInstance(
            handle,
            [](MaterialInstance& editable)
            {
                editable.baseColor.a = 0.5f;
                editable.renderState.alphaMode = MaterialAlphaMode::Blend;
                editable.renderState.depthWrite = false;
                return true;
            });
        const MaterialSyncResult alphaSync = materials.synchronizeGpuState(handle, &resources);
        const MaterialFrameState frameState = makeMaterialFrameState(*alphaSync.state);

        return require(firstSync.changed, "initial sync creates material GPU state")
            && require(parameterSync.parameterChanged && !parameterSync.topologyChanged,
                       "base color change is a material parameter update")
            && require(parameterVariant == initialVariant,
                       "base color change preserves material variant key")
            && require(topologySync.topologyChanged,
                       "double-sided change is material topology")
            && require(topologySync.state->variantKey != initialVariant,
                       "double-sided change updates material variant key")
            && require(alphaSync.topologyChanged,
                       "alpha mode change is material topology")
            && require(frameState.renderQueue == RenderQueue::Transparent,
                       "blend alpha mode maps to transparent render queue");
    }

    bool materialFrameDataUsesIndexedLookup()
    {
        MaterialFrameData frameData;
        frameData.reserve(4096);

        for (uint32_t i = 1; i <= 4096; ++i)
        {
            MaterialFrameState state;
            state.gpuHandle = MaterialGpuHandle{i, i + 10u};
            state.uploadedVersion = i;
            frameData.upsert(state);
        }

        MaterialFrameState replacement;
        replacement.gpuHandle = MaterialGpuHandle{2048, 2058};
        replacement.uploadedVersion = 9999;
        frameData.upsert(replacement);

        const MaterialFrameState* replaced = frameData.find(replacement.gpuHandle);
        const MaterialFrameState* first = frameData.find(MaterialGpuHandle{1, 11});
        const MaterialFrameState* last = frameData.find(MaterialGpuHandle{4096, 4106});

        frameData.clear();
        const bool cleared = frameData.find(replacement.gpuHandle) == nullptr;

        return require(replaced != nullptr && replaced->uploadedVersion == 9999,
                       "material frame data replaces an existing GPU handle through indexed lookup")
            && require(first != nullptr && first->uploadedVersion == 1,
                       "material frame data finds the first inserted material")
            && require(last != nullptr && last->uploadedVersion == 4096,
                       "material frame data finds the last inserted material")
            && require(cleared, "material frame data clear resets the lookup index");
    }

    bool standardSurfaceTextureRolesResolveWithColorSpace()
    {
        gts::rendering::MaterialRuntime materials;
        FakeResourceProvider resources;

        const MaterialInstance* defaultSurface =
            materials.getInstance(materials.defaultStandardSurfaceMaterial());
        MaterialInstance instance = defaultSurface != nullptr ? *defaultSurface : MaterialInstance{};
        instance.baseColorTexture = MaterialTextureBinding::assetPath("textures/shared.png");
        instance.metallicRoughnessTexture =
            MaterialTextureBinding::dataAssetPath("textures/shared.png");
        instance.normalTexture = MaterialTextureBinding::dataAssetPath("textures/normal.png");
        instance.ambientOcclusionTexture = MaterialTextureBinding::dataAssetPath("textures/ao.png");
        instance.emissiveTexture = MaterialTextureBinding::assetPath("textures/emissive.png");

        const MaterialInstanceHandle handle = materials.createInstance(instance);
        const MaterialSyncResult sync = materials.synchronizeGpuState(handle, &resources);
        const MaterialGpuState* state = sync.state;

        return require(sync.changed && state != nullptr, "standard surface texture set synchronizes")
            && require(state->textures.baseColor != 0 &&
                       state->textures.metallicRoughness != 0 &&
                       state->textures.normal != 0 &&
                       state->textures.ambientOcclusion != 0 &&
                       state->textures.emissive != 0,
                       "every standard-surface texture role resolves to an ID")
            && require(state->textures.baseColor != state->textures.metallicRoughness,
                       "same path can resolve differently for color and data usage")
            && require(resources.textureColorSpaces["textures/shared.png"] == TextureColorSpace::Linear,
                       "last same-path data request records linear color-space intent")
            && require(resources.textureColorSpaces["textures/normal.png"] == TextureColorSpace::Linear,
                       "normal texture requests are linear/data")
            && require(resources.textureColorSpaces["textures/ao.png"] == TextureColorSpace::Linear,
                       "ambient-occlusion texture requests are linear/data")
            && require(resources.textureColorSpaces["textures/emissive.png"] == TextureColorSpace::SRgb,
                       "emissive texture requests are sRGB")
            && require(hasMaterialFeature(state->featureFlags, MaterialFeatureFlags::HasBaseColorTexture) &&
                       hasMaterialFeature(state->featureFlags, MaterialFeatureFlags::HasMetallicRoughnessTexture) &&
                       hasMaterialFeature(state->featureFlags, MaterialFeatureFlags::HasNormalTexture) &&
                       hasMaterialFeature(state->featureFlags, MaterialFeatureFlags::HasAmbientOcclusionTexture) &&
                       hasMaterialFeature(state->featureFlags, MaterialFeatureFlags::HasEmissiveTexture),
                       "material GPU state records texture feature bits");
    }

    bool missingMapsResolveNeutralFallbacks()
    {
        gts::rendering::MaterialRuntime materials;
        FakeResourceProvider resources;

        const MaterialInstance* defaultSurface =
            materials.getInstance(materials.defaultStandardSurfaceMaterial());
        MaterialInstance instance = defaultSurface != nullptr ? *defaultSurface : MaterialInstance{};
        instance.baseColorTexture = MaterialTextureBinding::assetPath({});
        instance.metallicRoughnessTexture = MaterialTextureBinding::dataAssetPath({});
        instance.normalTexture = MaterialTextureBinding::dataAssetPath({});
        instance.ambientOcclusionTexture = MaterialTextureBinding::dataAssetPath({});
        instance.emissiveTexture = MaterialTextureBinding::assetPath({});

        const MaterialInstanceHandle handle = materials.createInstance(instance);
        const MaterialSyncResult sync = materials.synchronizeGpuState(handle, &resources);
        const MaterialGpuState* state = sync.state;

        return require(state != nullptr, "fallback material state exists")
            && require(state->textures.baseColor != 0 &&
                       state->textures.metallicRoughness != 0 &&
                       state->textures.normal != 0 &&
                       state->textures.ambientOcclusion != 0 &&
                       state->textures.emissive != 0,
                       "missing maps resolve through fallback textures")
            && require(!hasMaterialFeature(state->featureFlags, MaterialFeatureFlags::HasNormalTexture),
                       "fallback normal texture does not imply authored normal-map feature")
            && require(resources.textureColorSpaces["__fallback/engine_pbr_base_white.png"] == TextureColorSpace::SRgb,
                       "base-color fallback is sRGB")
            && require(resources.textureColorSpaces["__fallback/engine_pbr_metallic_roughness_neutral.png"] ==
                       TextureColorSpace::Linear,
                       "metallic-roughness fallback is linear")
            && require(resources.textureColorSpaces["__fallback/engine_pbr_normal_flat.png"] == TextureColorSpace::Linear,
                       "normal fallback is linear")
            && require(resources.textureColorSpaces["__fallback/engine_pbr_ao_white.png"] == TextureColorSpace::Linear,
                       "AO fallback is linear")
            && require(resources.textureColorSpaces["__fallback/engine_pbr_emissive_black.png"] == TextureColorSpace::SRgb,
                       "emissive fallback is sRGB");
    }

    bool surfaceFactorsAreVersionedWithoutChangingVariant()
    {
        gts::rendering::MaterialRuntime materials;
        FakeResourceProvider resources;

        const MaterialInstance* defaultSurface =
            materials.getInstance(materials.defaultStandardSurfaceMaterial());
        MaterialInstance instance = defaultSurface != nullptr ? *defaultSurface : MaterialInstance{};
        const MaterialInstanceHandle handle = materials.createInstance(instance);
        const MaterialSyncResult first = materials.synchronizeGpuState(handle, &resources);
        const MaterialVariantKey initialVariant = first.state->variantKey;
        const uint64_t initialVersion = materials.getInstance(handle)->version;

        materials.modifyInstance(
            handle,
            [](MaterialInstance& editable)
            {
                editable.metallic = 2.0f;
                editable.roughness = 0.0f;
                editable.normalScale = std::numeric_limits<float>::quiet_NaN();
                editable.ambientOcclusionStrength = 2.0f;
                editable.emissiveFactor = {-1.0f, 0.5f, 3.0f};
                editable.emissiveStrength = -4.0f;
                return true;
            });
        const MaterialSyncResult second = materials.synchronizeGpuState(handle, &resources);

        return require(materials.getInstance(handle)->version == initialVersion + 1,
                       "surface factor edit increments material version")
            && require(second.parameterChanged && !second.topologyChanged,
                       "surface factor edit is parameter-only")
            && require(second.state->variantKey == initialVariant,
                       "surface factor edit preserves variant key")
            && require(near(second.state->parameters.metallic(), 1.0f) &&
                       near(second.state->parameters.roughness(), MaterialMinimumRoughness) &&
                       near(second.state->parameters.normalScale(), 1.0f) &&
                       near(second.state->parameters.ambientOcclusionStrength(), 1.0f) &&
                       near(second.state->parameters.emissiveFactor().x, 0.0f) &&
                       near(second.state->parameters.emissiveFactor().y, 0.5f) &&
                       near(second.state->parameters.emissiveFactor().z, 3.0f) &&
                       near(second.state->parameters.emissiveStrength(), 0.0f),
                       "surface factors are sanitized into GPU parameter layout");
    }

    bool normalMapPresenceControlsVariantButTextureReplacementDoesNot()
    {
        gts::rendering::MaterialRuntime materials;
        FakeResourceProvider resources;

        const MaterialInstance* defaultSurface =
            materials.getInstance(materials.defaultStandardSurfaceMaterial());
        MaterialInstance instance = defaultSurface != nullptr ? *defaultSurface : MaterialInstance{};
        const MaterialInstanceHandle handle = materials.createInstance(instance);
        const MaterialSyncResult first = materials.synchronizeGpuState(handle, &resources);
        const MaterialVariantKey initialVariant = first.state->variantKey;

        materials.modifyInstance(
            handle,
            [](MaterialInstance& editable)
            {
                editable.metallicRoughnessTexture =
                    MaterialTextureBinding::dataAssetPath("textures/mr_a.png");
                return true;
            });
        const MaterialSyncResult metallicRoughnessSync =
            materials.synchronizeGpuState(handle, &resources);
        const bool metallicRoughnessParameterChanged = metallicRoughnessSync.parameterChanged;
        const bool metallicRoughnessTopologyChanged = metallicRoughnessSync.topologyChanged;
        const MaterialVariantKey metallicRoughnessVariant =
            metallicRoughnessSync.state->variantKey;

        materials.modifyInstance(
            handle,
            [](MaterialInstance& editable)
            {
                editable.normalTexture =
                    MaterialTextureBinding::dataAssetPath("textures/normal_a.png");
                return true;
            });
        const MaterialSyncResult normalEnabledSync =
            materials.synchronizeGpuState(handle, &resources);
        const bool normalEnabledTopologyChanged = normalEnabledSync.topologyChanged;
        const MaterialVariantKey normalVariant = normalEnabledSync.state->variantKey;

        materials.modifyInstance(
            handle,
            [](MaterialInstance& editable)
            {
                editable.normalTexture =
                    MaterialTextureBinding::dataAssetPath("textures/normal_b.png");
                return true;
            });
        const MaterialSyncResult normalReplacementSync =
            materials.synchronizeGpuState(handle, &resources);
        const bool normalReplacementParameterChanged = normalReplacementSync.parameterChanged;
        const bool normalReplacementTopologyChanged = normalReplacementSync.topologyChanged;
        const MaterialVariantKey normalReplacementVariant =
            normalReplacementSync.state->variantKey;

        return require(metallicRoughnessParameterChanged &&
                       !metallicRoughnessTopologyChanged,
                       "metallic-roughness texture presence is parameter data in current variant policy")
            && require(metallicRoughnessVariant == initialVariant,
                       "metallic-roughness texture presence does not alter variant key")
            && require(normalEnabledTopologyChanged,
                       "normal-map presence is topology-relevant")
            && require(normalVariant != initialVariant,
                       "normal-map presence updates variant key")
            && require(normalReplacementParameterChanged &&
                       !normalReplacementTopologyChanged,
                       "normal texture replacement is parameter-only while feature remains enabled")
            && require(normalReplacementVariant == normalVariant,
                       "normal texture replacement preserves variant key");
    }

    bool commandExtractorSortsQueuesAndTransparentDepth()
    {
        RenderExtractionSnapshot snapshot;
        snapshot.cameraViewID = 1;
        snapshot.contentVersion = 1;
        snapshot.visibilityVersion = 1;
        snapshot.cameraVersion = 1;

        RenderableSnapshot opaque;
        opaque.objectSSBOSlot = 0;
        opaque.meshID = 20;
        opaque.material = {1, 1};
        opaque.materialGpu = {1, 1};
        opaque.variantKey = {1};
        opaque.renderQueue = RenderQueue::Opaque;
        opaque.sortKey = 10;
        opaque.cameraDepth = 1.0f;

        RenderableSnapshot transparentNear = opaque;
        transparentNear.objectSSBOSlot = 1;
        transparentNear.material = {2, 1};
        transparentNear.materialGpu = {2, 1};
        transparentNear.variantKey = {2};
        transparentNear.renderQueue = RenderQueue::Transparent;
        transparentNear.sortKey = 20;
        transparentNear.cameraDepth = 2.0f;

        RenderableSnapshot transparentFar = transparentNear;
        transparentFar.objectSSBOSlot = 2;
        transparentFar.material = {3, 1};
        transparentFar.materialGpu = {3, 1};
        transparentFar.sortKey = 30;
        transparentFar.cameraDepth = 8.0f;

        snapshot.renderables = {transparentNear, opaque, transparentFar};

        RenderCommandExtractor extractor;
        const std::vector<RenderCommand>& commands = extractor.extract(snapshot);

        return require(commands.size() == 3, "extractor emits every visible command")
            && require(commands[0].renderQueue == RenderQueue::Opaque,
                       "opaque commands are submitted before transparent commands")
            && require(commands[1].objectSSBOSlot == transparentFar.objectSSBOSlot,
                       "transparent commands sort far to near")
            && require(commands[2].objectSSBOSlot == transparentNear.objectSSBOSlot,
                       "nearest transparent command is submitted last");
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
        const auto materialMetrics = MaterialBindingSystem::getLastMetrics();

        return require(world.getComponent<MaterialReferenceComponent>(entity).material == materials.defaultMaterial(),
                       "destroyed material references are replaced with the default material")
            && require(materials.getGpuState(materials.defaultMaterial()) != nullptr,
                       "default fallback material has synchronized GPU state")
            && require(materialMetrics.fallbackSubstitutions == 1 &&
                       materialMetrics.userInvalidations == 1,
                       "destroyed material performs one indexed fallback substitution")
            && require(world.hasComponent<RenderGpuComponent>(entity),
                       "fallback material keeps render object valid");
    }
}

int main()
{
    bool ok = true;
    ok &= handlesVersionsAndClone();
    ok &= materialUserIndexTracksAddRemoveReplaceAndReset();
    ok &= materialReferenceCallbacksMaintainUserIndex();
    ok &= sharedMaterialUpdatesAllReferencingRenderables();
    ok &= steadyStateMaterialBindingDoesNoSynchronizationWork();
    ok &= legacyAndWorldTextDescriptorsQueueMaterialSync();
    ok &= topologyChangeInvalidatesOnlyIndexedUsers();
    ok &= extractionCarriesStableMaterialIdentity();
    ok &= renderCommandsUseMaterialIdentity();
    ok &= variantKeySeparatesTopologyFromParameters();
    ok &= materialFrameDataUsesIndexedLookup();
    ok &= standardSurfaceTextureRolesResolveWithColorSpace();
    ok &= missingMapsResolveNeutralFallbacks();
    ok &= surfaceFactorsAreVersionedWithoutChangingVariant();
    ok &= normalMapPresenceControlsVariantButTextureReplacementDoesNot();
    ok &= commandExtractorSortsQueuesAndTransparentDepth();
    ok &= perObjectUvAnimationDoesNotMutateSharedMaterial();
    ok &= destroyedSharedMaterialFallsBackSafely();

    if (!ok)
        return 1;

    std::printf("MaterialRuntimeArchitectureTest passed\n");
    return 0;
}
