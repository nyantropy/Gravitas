#include <cstdio>
#include <string>
#include <unordered_map>
#include <vector>

#include "BitmapFont.h"
#include "BoundsComponent.h"
#include "DynamicMeshComponent.h"
#include "ECSWorld.hpp"
#include "EcsControllerContext.hpp"
#include "GeometryBindingLifecycle.h"
#include "IResourceProvider.hpp"
#include "MaterialReferenceHelpers.h"
#include "MaterialReferenceComponent.h"
#include "MaterialRuntime.h"
#include "MeshGpuComponent.h"
#include "QuadMeshComponent.h"
#include "RenderGpuComponent.h"
#include "RenderGpuSystem.hpp"
#include "RendererGeometrySceneFeature.h"
#include "StaticMeshComponent.h"
#include "TransformComponent.h"
#include "TransformDirtyHelpers.h"
#include "TransformSceneFeature.h"
#include "Vertex.h"
#include "WorldTextComponent.h"
#include "WorldTransformComponent.h"

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
        uint32_t quadRequests = 0;
        uint32_t proceduralUploads = 0;
        uint32_t proceduralReleases = 0;
        uint32_t textureRequests = 0;
        uint32_t objectSlotRequests = 0;
        uint32_t objectSlotReleases = 0;
        bool failProceduralUploads = false;

        std::unordered_map<std::string, mesh_id_type> meshes;
        std::unordered_map<std::string, texture_id_type> textures;
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

        mesh_id_type getSharedQuadMesh(float w, float h) override
        {
            ++quadRequests;
            return static_cast<mesh_id_type>(4000 + static_cast<int>(w * 10.0f) * 100 + static_cast<int>(h * 10.0f));
        }

        mesh_id_type uploadProceduralMesh(mesh_id_type existingId,
                                          const std::vector<Vertex>&,
                                          const std::vector<uint32_t>&,
                                          VertexAttributeFlags = UnlitVertexAttributes) override
        {
            ++proceduralUploads;
            if (failProceduralUploads)
                return 0;
            if (existingId != 0)
                return existingId;
            return nextMesh++;
        }

        void releaseProceduralMesh(mesh_id_type) override
        {
            ++proceduralReleases;
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
            ++objectSlotReleases;
        }
    };

    void install(ECSWorld& world, FakeResourceProvider& resources)
    {
        gts::transform::installTransformFeature(world);
        gts::rendering::installRendererGeometrySceneFeature(world, &resources);
    }

    void update(ECSWorld& world, FakeResourceProvider& resources)
    {
        EcsControllerContext ctx{world};
        ctx.resources = &resources;
        world.updateControllers(ctx);
    }

    TransformComponent transformAt(const glm::vec3& position)
    {
        TransformComponent transform;
        transform.position = position;
        return transform;
    }

    MaterialReferenceComponent materialWithTexture(ECSWorld& world, const std::string& path)
    {
        gts::rendering::UnlitMaterialDescriptor material;
        material.texturePath = path;
        return MaterialReferenceComponent{gts::rendering::createUnlitMaterial(world, material)};
    }

    const MaterialGpuState* materialState(ECSWorld& world, Entity entity)
    {
        if (!world.hasComponent<MaterialReferenceComponent>(entity))
            return nullptr;

        return gts::rendering::materialRuntime(world).getGpuState(
            world.getComponent<MaterialReferenceComponent>(entity).material);
    }

    bool hasSyncedMaterial(ECSWorld& world, Entity entity)
    {
        const MaterialGpuState* state = materialState(world, entity);
        return state != nullptr && state->baseColorTextureID != 0;
    }

    texture_id_type materialTextureID(ECSWorld& world, Entity entity)
    {
        const MaterialGpuState* state = materialState(world, entity);
        return state == nullptr ? 0 : state->baseColorTextureID;
    }

    DynamicMeshComponent triangleMesh(uint64_t version)
    {
        DynamicMeshComponent mesh;
        mesh.geometryVersion = version;
        mesh.vertices = {
            Vertex{{0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f}},
            Vertex{{1.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {1.0f, 0.0f}},
            Vertex{{0.0f, 1.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}}
        };
        mesh.indices = {0, 1, 2};
        return mesh;
    }

    Entity createRenderable(ECSWorld& world,
                            const StaticMeshComponent& mesh,
                            const MaterialReferenceComponent& material)
    {
        Entity entity = world.createEntity();
        world.addComponent(entity, transformAt({0.0f, 0.0f, 0.0f}));
        world.addComponent(entity, mesh);
        world.addComponent(entity, material);
        return entity;
    }

    bool staticMeshMaterialAndRenderObjectSplit()
    {
        ECSWorld world;
        FakeResourceProvider resources;
        install(world, resources);

        StaticMeshComponent mesh;
        mesh.meshPath = "mesh/a.obj";
        const Entity entity = createRenderable(world, mesh, materialWithTexture(world, "textures/a.png"));
        update(world, resources);

        return require(world.hasComponent<MeshGpuComponent>(entity), "static mesh creates MeshGpuComponent")
            && require(world.hasComponent<MaterialReferenceComponent>(entity), "material creates MaterialReferenceComponent")
            && require(hasSyncedMaterial(world, entity), "material reference has synchronized GPU cache state")
            && require(world.hasComponent<RenderGpuComponent>(entity), "render object lifecycle creates RenderGpuComponent")
            && require(world.getComponent<RenderGpuComponent>(entity).readyToRender, "render object becomes ready")
            && require(resources.meshRequests == 1, "static mesh requested once")
            && require(resources.textureRequests == 5, "material texture role set requested once")
            && require(resources.objectSlotRequests == 1, "render object slot allocated once");
    }

    bool materialChangeDoesNotRecreateMesh()
    {
        ECSWorld world;
        FakeResourceProvider resources;
        install(world, resources);

        StaticMeshComponent mesh;
        mesh.meshPath = "mesh/a.obj";
        const Entity entity = createRenderable(world, mesh, materialWithTexture(world, "textures/a.png"));
        update(world, resources);

        const mesh_id_type meshId = world.getComponent<MeshGpuComponent>(entity).meshID;
        MaterialReferenceComponent& material = world.getComponent<MaterialReferenceComponent>(entity);
        gts::rendering::materialRuntime(world).modifyInstance(
            material.material,
            [](MaterialInstance& instance)
            {
                instance.baseColor = {0.25f, 0.5f, 0.75f, 1.0f};
                return true;
            });
        update(world, resources);

        return require(world.getComponent<MeshGpuComponent>(entity).meshID == meshId,
                       "material change preserves mesh GPU state")
            && require(resources.meshRequests == 1, "material change does not request mesh")
            && require(resources.textureRequests == 5, "tint-only material change does not request texture roles");
    }

    bool meshChangeDoesNotRecreateMaterial()
    {
        ECSWorld world;
        FakeResourceProvider resources;
        install(world, resources);

        StaticMeshComponent mesh;
        mesh.meshPath = "mesh/a.obj";
        const Entity entity = createRenderable(world, mesh, materialWithTexture(world, "textures/a.png"));
        update(world, resources);

        const texture_id_type textureId = materialTextureID(world, entity);
        StaticMeshComponent& staticMesh = world.getComponent<StaticMeshComponent>(entity);
        staticMesh.meshPath = "mesh/b.obj";
        gts::rendering::queueStaticMeshRefresh(world, entity);
        update(world, resources);

        return require(materialTextureID(world, entity) == textureId,
                       "mesh change preserves material GPU state")
            && require(resources.meshRequests == 2, "mesh change requests new mesh")
            && require(resources.textureRequests == 5, "mesh change does not request texture roles again");
    }

    bool renderObjectWaitsForMaterialCompanion()
    {
        ECSWorld world;
        FakeResourceProvider resources;
        install(world, resources);

        Entity entity = world.createEntity();
        StaticMeshComponent mesh;
        mesh.meshPath = "mesh/a.obj";
        world.addComponent(entity, transformAt({0.0f, 0.0f, 0.0f}));
        world.addComponent(entity, mesh);
        update(world, resources);

        const bool meshReadyWithoutRenderObject =
            world.hasComponent<MeshGpuComponent>(entity)
            && !world.hasComponent<MaterialReferenceComponent>(entity)
            && !world.hasComponent<RenderGpuComponent>(entity);

        world.addComponent(entity, materialWithTexture(world, "textures/a.png"));
        update(world, resources);

        return require(meshReadyWithoutRenderObject, "mesh alone does not create render object")
            && require(world.hasComponent<RenderGpuComponent>(entity), "render object appears after material binding");
    }

    bool descriptorRemovalReleasesObjectSlotOnce()
    {
        ECSWorld world;
        FakeResourceProvider resources;
        install(world, resources);

        StaticMeshComponent mesh;
        mesh.meshPath = "mesh/a.obj";
        const Entity entity = createRenderable(world, mesh, materialWithTexture(world, "textures/a.png"));
        update(world, resources);

        world.removeComponent<StaticMeshComponent>(entity);
        update(world, resources);
        update(world, resources);

        return require(!world.hasComponent<MeshGpuComponent>(entity), "geometry removal tears down mesh companion")
            && require(world.hasComponent<MaterialReferenceComponent>(entity), "material reference survives while material descriptor remains")
            && require(hasSyncedMaterial(world, entity), "material cache survives while material descriptor remains")
            && require(!world.hasComponent<RenderGpuComponent>(entity), "render object removed when geometry is gone")
            && require(resources.objectSlotReleases == 1, "object slot released exactly once");
    }

    bool dynamicMeshVersionPreservesMaterial()
    {
        ECSWorld world;
        FakeResourceProvider resources;
        install(world, resources);

        Entity entity = world.createEntity();
        world.addComponent(entity, transformAt({0.0f, 0.0f, 0.0f}));
        world.addComponent(entity, triangleMesh(1));
        world.addComponent(entity, materialWithTexture(world, "textures/a.png"));
        update(world, resources);

        const texture_id_type textureId = materialTextureID(world, entity);
        DynamicMeshComponent& mesh = world.getComponent<DynamicMeshComponent>(entity);
        mesh = triangleMesh(2);
        gts::rendering::queueDynamicMeshGeometryRefresh(world, entity);
        update(world, resources);

        return require(materialTextureID(world, entity) == textureId,
                       "dynamic mesh update preserves material")
            && require(resources.proceduralUploads == 2, "dynamic mesh version uploads new geometry")
            && require(resources.textureRequests == 5, "dynamic mesh update does not request material texture roles again");
    }

    bool dynamicMeshUnchangedVersionSkipsUpload()
    {
        ECSWorld world;
        FakeResourceProvider resources;
        install(world, resources);

        Entity entity = world.createEntity();
        world.addComponent(entity, transformAt({0.0f, 0.0f, 0.0f}));
        world.addComponent(entity, triangleMesh(1));
        world.addComponent(entity, materialWithTexture(world, "textures/a.png"));
        update(world, resources);

        gts::rendering::queueDynamicMeshGeometryRefresh(world, entity);
        update(world, resources);

        const MeshGpuComponent& meshGpu = world.getComponent<MeshGpuComponent>(entity);
        return require(resources.proceduralUploads == 1, "unchanged dynamic mesh version skips upload")
            && require(meshGpu.boundDynamicGeometryVersion == 1, "unchanged dynamic mesh keeps uploaded version");
    }

    bool dynamicMeshMutationHelperDeduplicatesAndAvoidsMaterialRefresh()
    {
        ECSWorld world;
        FakeResourceProvider resources;
        install(world, resources);

        Entity entity = world.createEntity();
        world.addComponent(entity, transformAt({0.0f, 0.0f, 0.0f}));
        world.addComponent(entity, triangleMesh(1));
        world.addComponent(entity, materialWithTexture(world, "textures/a.png"));
        update(world, resources);

        DynamicMeshComponent& mesh = world.getComponent<DynamicMeshComponent>(entity);
        mesh.vertices[1].pos.x += 0.25f;
        gts::rendering::markDynamicMeshChanged(world, entity);
        mesh.vertices[1].pos.x += 0.25f;
        gts::rendering::markDynamicMeshChanged(world, entity);
        update(world, resources);

        const MeshGpuComponent& meshGpu = world.getComponent<MeshGpuComponent>(entity);
        return require(resources.proceduralUploads == 2, "repeated dynamic mesh mutations queue one upload")
            && require(meshGpu.boundDynamicGeometryVersion == mesh.geometryVersion,
                       "dynamic mesh mutation publishes final geometry version")
            && require(resources.textureRequests == 5,
                       "dynamic mesh mutation does not refresh material textures");
    }

    bool dynamicMeshCapacityMetadataReusesExistingMesh()
    {
        ECSWorld world;
        FakeResourceProvider resources;
        install(world, resources);

        Entity entity = world.createEntity();
        world.addComponent(entity, transformAt({0.0f, 0.0f, 0.0f}));
        world.addComponent(entity, triangleMesh(1));
        world.addComponent(entity, materialWithTexture(world, "textures/a.png"));
        update(world, resources);

        const MeshGpuComponent initialGpu = world.getComponent<MeshGpuComponent>(entity);
        DynamicMeshComponent& mesh = world.getComponent<DynamicMeshComponent>(entity);
        mesh.vertices[2].pos.y += 0.5f;
        gts::rendering::markDynamicMeshChanged(world, entity);
        update(world, resources);

        const MeshGpuComponent& updatedGpu = world.getComponent<MeshGpuComponent>(entity);
        return require(updatedGpu.meshID == initialGpu.meshID,
                       "capacity-stable dynamic mesh update reuses mesh identity")
            && require(updatedGpu.dynamicVertexCapacityBytes == initialGpu.dynamicVertexCapacityBytes,
                       "capacity-stable dynamic mesh update keeps vertex capacity")
            && require(updatedGpu.dynamicIndexCapacityBytes == initialGpu.dynamicIndexCapacityBytes,
                       "capacity-stable dynamic mesh update keeps index capacity")
            && require(updatedGpu.dynamicVertexUsedBytes == initialGpu.dynamicVertexUsedBytes,
                       "capacity-stable dynamic mesh update tracks used vertex bytes")
            && require(updatedGpu.dynamicIndexUsedBytes == initialGpu.dynamicIndexUsedBytes,
                       "capacity-stable dynamic mesh update tracks used index bytes");
    }

    bool dynamicMeshMutationUpdatesBounds()
    {
        ECSWorld world;
        FakeResourceProvider resources;
        install(world, resources);

        Entity entity = world.createEntity();
        world.addComponent(entity, transformAt({0.0f, 0.0f, 0.0f}));
        world.addComponent(entity, triangleMesh(1));
        world.addComponent(entity, BoundsComponent{});
        world.addComponent(entity, materialWithTexture(world, "textures/a.png"));
        update(world, resources);

        DynamicMeshComponent& mesh = world.getComponent<DynamicMeshComponent>(entity);
        mesh.vertices[1].pos.x = 2.0f;
        gts::rendering::markDynamicMeshChanged(world, entity);
        update(world, resources);

        const BoundsComponent& bounds = world.getComponent<BoundsComponent>(entity);
        return require(bounds.min.x == 0.0f, "dynamic mesh bounds min is recomputed")
            && require(bounds.max.x == 2.0f, "dynamic mesh bounds max is recomputed");
    }

    bool dynamicMeshInvalidVersionDoesNotRetry()
    {
        ECSWorld world;
        FakeResourceProvider resources;
        install(world, resources);

        Entity entity = world.createEntity();
        world.addComponent(entity, transformAt({0.0f, 0.0f, 0.0f}));
        world.addComponent(entity, triangleMesh(1));
        world.addComponent(entity, materialWithTexture(world, "textures/a.png"));
        update(world, resources);

        DynamicMeshComponent& mesh = world.getComponent<DynamicMeshComponent>(entity);
        mesh.indices = {0, 1, 99};
        gts::rendering::markDynamicMeshChanged(world, entity);
        const uint64_t failedVersion = mesh.geometryVersion;
        update(world, resources);

        const uint32_t releasesAfterFailure = resources.proceduralReleases;
        gts::rendering::queueDynamicMeshGeometryRefresh(world, entity);
        update(world, resources);

        const MeshGpuComponent& meshGpu = world.getComponent<MeshGpuComponent>(entity);
        return require(meshGpu.meshID == 0, "invalid dynamic mesh version leaves mesh unready")
            && require(meshGpu.attemptedDynamicGeometryVersion == failedVersion,
                       "invalid dynamic mesh records attempted version")
            && require(resources.proceduralUploads == 1,
                       "invalid dynamic mesh version does not upload geometry")
            && require(resources.proceduralReleases == releasesAfterFailure,
                       "invalid dynamic mesh version does not retry cleanup");
    }

    bool dynamicMeshUploadFailureDoesNotRetry()
    {
        ECSWorld world;
        FakeResourceProvider resources;
        install(world, resources);

        Entity entity = world.createEntity();
        world.addComponent(entity, transformAt({0.0f, 0.0f, 0.0f}));
        world.addComponent(entity, triangleMesh(1));
        world.addComponent(entity, materialWithTexture(world, "textures/a.png"));
        update(world, resources);

        DynamicMeshComponent& mesh = world.getComponent<DynamicMeshComponent>(entity);
        mesh.vertices[1].pos.x += 0.25f;
        resources.failProceduralUploads = true;
        gts::rendering::markDynamicMeshChanged(world, entity);
        const uint64_t failedVersion = mesh.geometryVersion;
        update(world, resources);

        const uint32_t uploadsAfterFailure = resources.proceduralUploads;
        gts::rendering::queueDynamicMeshGeometryRefresh(world, entity);
        update(world, resources);

        const MeshGpuComponent& meshGpu = world.getComponent<MeshGpuComponent>(entity);
        return require(meshGpu.meshID == 0, "failed dynamic mesh upload leaves mesh unready")
            && require(meshGpu.attemptedDynamicGeometryVersion == failedVersion,
                       "failed dynamic mesh upload records attempted version")
            && require(resources.proceduralUploads == uploadsAfterFailure,
                       "failed dynamic mesh upload is not retried without a new version");
    }

    bool geometryDescriptorTypeChangeKeepsObjectSlot()
    {
        ECSWorld world;
        FakeResourceProvider resources;
        install(world, resources);

        StaticMeshComponent mesh;
        mesh.meshPath = "mesh/a.obj";
        const Entity entity = createRenderable(world, mesh, materialWithTexture(world, "textures/a.png"));
        update(world, resources);
        const ssbo_id_type slot = world.getComponent<RenderGpuComponent>(entity).objectSSBOSlot;

        QuadMeshComponent quad;
        quad.width = 2.0f;
        quad.height = 3.0f;
        world.removeComponent<StaticMeshComponent>(entity);
        world.addComponent(entity, quad);
        update(world, resources);

        return require(world.hasComponent<MeshGpuComponent>(entity), "quad descriptor rebinds mesh companion")
            && require(world.hasComponent<RenderGpuComponent>(entity), "render object survives descriptor type change")
            && require(world.getComponent<RenderGpuComponent>(entity).objectSSBOSlot == slot,
                       "descriptor type change keeps object slot")
            && require(resources.objectSlotReleases == 0, "descriptor type change does not release live object slot");
    }

    bool staticRenderableDoesNotScanOrUploadInSteadyState()
    {
        ECSWorld world;
        FakeResourceProvider resources;
        install(world, resources);

        StaticMeshComponent mesh;
        mesh.meshPath = "mesh/a.obj";
        const Entity entity = createRenderable(world, mesh, materialWithTexture(world, "textures/a.png"));
        update(world, resources);
        update(world, resources);

        const RenderGpuSystem::Metrics metrics = RenderGpuSystem::getLastMetrics();
        return require(world.hasComponent<RenderGpuComponent>(entity), "static control has render GPU state")
            && require(metrics.fullScanRenderablesVisited == 0, "steady-state RenderGpuSystem performs no full scan")
            && require(metrics.queuedEntriesDrained == 0, "steady-state RenderGpuSystem drains no queued transforms")
            && require(metrics.updatedRenderables == 0, "steady-state RenderGpuSystem emits no transform uploads");
    }

    bool movedRenderableQueuesOneRenderSync()
    {
        ECSWorld world;
        FakeResourceProvider resources;
        install(world, resources);

        StaticMeshComponent mesh;
        mesh.meshPath = "mesh/a.obj";
        const Entity entity = createRenderable(world, mesh, materialWithTexture(world, "textures/a.png"));
        update(world, resources);

        TransformComponent& transform = world.getComponent<TransformComponent>(entity);
        transform.position.x += 2.0f;
        gts::transform::markDirty(world, entity);
        update(world, resources);

        const RenderGpuSystem::Metrics metrics = RenderGpuSystem::getLastMetrics();
        const RenderGpuComponent& renderGpu = world.getComponent<RenderGpuComponent>(entity);
        const WorldTransformComponent& worldTransform = world.getComponent<WorldTransformComponent>(entity);
        return require(metrics.fullScanRenderablesVisited == 0, "moving render sync does not fall back to full scan")
            && require(metrics.queuedEntriesDrained == 1, "moving renderable drains one queued entry")
            && require(metrics.updatedRenderables == 1, "moving renderable synchronizes once")
            && require(metrics.modelMatricesCopied == 1, "moving renderable copies one model matrix")
            && require(metrics.objectUploadRequests == 1, "moving renderable requests one object upload")
            && require(renderGpu.uploadedWorldTransformVersion == worldTransform.version,
                       "render GPU uploaded version matches world transform version");
    }

    bool renderTransformQueueDeduplicatesRepeatedScheduling()
    {
        ECSWorld world;
        FakeResourceProvider resources;
        install(world, resources);

        StaticMeshComponent mesh;
        mesh.meshPath = "mesh/a.obj";
        const Entity entity = createRenderable(world, mesh, materialWithTexture(world, "textures/a.png"));
        update(world, resources);

        gts::rendering::queueRenderTransformSync(world, entity);
        gts::rendering::queueRenderTransformSync(world, entity);
        update(world, resources);

        const RenderGpuSystem::Metrics metrics = RenderGpuSystem::getLastMetrics();
        return require(metrics.queuedEntriesDrained == 1, "duplicate render sync scheduling drains one entity")
            && require(metrics.queueDeduplications == 1, "duplicate render sync scheduling is counted")
            && require(metrics.unchangedVersionsSkipped == 1, "unchanged queued renderable is skipped by version")
            && require(metrics.updatedRenderables == 0, "unchanged queued renderable emits no upload");
    }

    bool readyLateRenderableReceivesInitialTransformSync()
    {
        ECSWorld world;
        FakeResourceProvider resources;
        install(world, resources);

        Entity entity = world.createEntity();
        StaticMeshComponent mesh;
        mesh.meshPath = "mesh/a.obj";
        world.addComponent(entity, transformAt({0.0f, 0.0f, 0.0f}));
        world.addComponent(entity, mesh);
        update(world, resources);

        world.addComponent(entity, materialWithTexture(world, "textures/a.png"));
        update(world, resources);

        const RenderGpuSystem::Metrics metrics = RenderGpuSystem::getLastMetrics();
        const RenderGpuComponent& renderGpu = world.getComponent<RenderGpuComponent>(entity);
        const WorldTransformComponent& worldTransform = world.getComponent<WorldTransformComponent>(entity);
        return require(metrics.updatedRenderables == 1,
                       "late-ready renderable receives initial render transform sync")
            && require(metrics.objectUploadRequests == 1,
                       "late-ready renderable requests initial object upload")
            && require(renderGpu.uploadedWorldTransformVersion == worldTransform.version,
                       "late-ready uploaded version matches current world transform");
    }

    bool staleRenderTransformQueueEntryIsDiscarded()
    {
        ECSWorld world;
        FakeResourceProvider resources;
        install(world, resources);

        const Entity entity = world.createEntity();
        gts::rendering::queueRenderTransformSync(world, entity);
        update(world, resources);

        const RenderGpuSystem::Metrics metrics = RenderGpuSystem::getLastMetrics();
        return require(metrics.queuedEntriesDrained == 1, "stale render sync entry is drained once")
            && require(metrics.staleQueueEntriesSkipped == 1, "stale render sync entry is counted")
            && require(metrics.missingComponentEntriesSkipped == 1, "stale render sync entry has missing components")
            && require(metrics.updatedRenderables == 0, "stale render sync entry emits no upload");
    }

    bool worldTextUsesMeshAndMaterialLifecycle()
    {
        ECSWorld world;
        FakeResourceProvider resources;
        install(world, resources);

        Entity entity = world.createEntity();
        WorldTextComponent text;
        text.text = "A";
        text.fontPath = "fonts/main.fnt";
        text.scale = 1.0f;
        world.addComponent(entity, transformAt({0.0f, 0.0f, 0.0f}));
        world.addComponent(entity, text);
        world.addComponent(entity, BoundsComponent{});
        update(world, resources);

        const texture_id_type atlas = materialTextureID(world, entity);
        WorldTextComponent& editableText = world.getComponent<WorldTextComponent>(entity);
        editableText.text = "AB";
        update(world, resources);

        return require(world.hasComponent<MeshGpuComponent>(entity), "world text creates mesh companion")
            && require(world.hasComponent<MaterialReferenceComponent>(entity), "world text creates material reference")
            && require(hasSyncedMaterial(world, entity), "world text creates synchronized material cache")
            && require(world.hasComponent<RenderGpuComponent>(entity), "world text creates render object")
            && require(materialTextureID(world, entity) == atlas,
                       "world text rebuild preserves atlas material")
            && require(resources.proceduralUploads == 2, "world text text change rebuilds procedural mesh")
            && require(resources.objectSlotRequests == 1, "world text keeps one object slot");
    }
}

int main()
{
    bool ok = true;
    ok &= staticMeshMaterialAndRenderObjectSplit();
    ok &= materialChangeDoesNotRecreateMesh();
    ok &= meshChangeDoesNotRecreateMaterial();
    ok &= renderObjectWaitsForMaterialCompanion();
    ok &= descriptorRemovalReleasesObjectSlotOnce();
    ok &= dynamicMeshVersionPreservesMaterial();
    ok &= dynamicMeshUnchangedVersionSkipsUpload();
    ok &= dynamicMeshMutationHelperDeduplicatesAndAvoidsMaterialRefresh();
    ok &= dynamicMeshCapacityMetadataReusesExistingMesh();
    ok &= dynamicMeshMutationUpdatesBounds();
    ok &= dynamicMeshInvalidVersionDoesNotRetry();
    ok &= dynamicMeshUploadFailureDoesNotRetry();
    ok &= geometryDescriptorTypeChangeKeepsObjectSlot();
    ok &= staticRenderableDoesNotScanOrUploadInSteadyState();
    ok &= movedRenderableQueuesOneRenderSync();
    ok &= renderTransformQueueDeduplicatesRepeatedScheduling();
    ok &= readyLateRenderableReceivesInitialTransformSync();
    ok &= staleRenderTransformQueueEntryIsDiscarded();
    ok &= worldTextUsesMeshAndMaterialLifecycle();

    if (!ok)
        return 1;

    std::printf("RenderLifecycleOwnershipTest passed\n");
    return 0;
}
