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
#include "MaterialComponent.h"
#include "MaterialReferenceComponent.h"
#include "MaterialRuntime.h"
#include "MeshGpuComponent.h"
#include "QuadMeshComponent.h"
#include "RenderGpuComponent.h"
#include "RendererGeometrySceneFeature.h"
#include "StaticMeshComponent.h"
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
                                          const std::vector<uint32_t>&) override
        {
            ++proceduralUploads;
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

    MaterialComponent materialWithTexture(const std::string& path)
    {
        MaterialComponent material;
        material.texturePath = path;
        return material;
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
                            const MaterialComponent& material)
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
        const Entity entity = createRenderable(world, mesh, materialWithTexture("textures/a.png"));
        update(world, resources);

        return require(world.hasComponent<MeshGpuComponent>(entity), "static mesh creates MeshGpuComponent")
            && require(world.hasComponent<MaterialReferenceComponent>(entity), "material creates MaterialReferenceComponent")
            && require(hasSyncedMaterial(world, entity), "material reference has synchronized GPU cache state")
            && require(world.hasComponent<RenderGpuComponent>(entity), "render object lifecycle creates RenderGpuComponent")
            && require(world.getComponent<RenderGpuComponent>(entity).readyToRender, "render object becomes ready")
            && require(resources.meshRequests == 1, "static mesh requested once")
            && require(resources.textureRequests == 1, "material texture requested once")
            && require(resources.objectSlotRequests == 1, "render object slot allocated once");
    }

    bool materialChangeDoesNotRecreateMesh()
    {
        ECSWorld world;
        FakeResourceProvider resources;
        install(world, resources);

        StaticMeshComponent mesh;
        mesh.meshPath = "mesh/a.obj";
        const Entity entity = createRenderable(world, mesh, materialWithTexture("textures/a.png"));
        update(world, resources);

        const mesh_id_type meshId = world.getComponent<MeshGpuComponent>(entity).meshID;
        MaterialComponent& material = world.getComponent<MaterialComponent>(entity);
        material.tint = {0.25f, 0.5f, 0.75f, 1.0f};
        gts::rendering::queueMaterialRefresh(world, entity);
        update(world, resources);

        return require(world.getComponent<MeshGpuComponent>(entity).meshID == meshId,
                       "material change preserves mesh GPU state")
            && require(resources.meshRequests == 1, "material change does not request mesh")
            && require(resources.textureRequests == 1, "tint-only material change does not request texture");
    }

    bool meshChangeDoesNotRecreateMaterial()
    {
        ECSWorld world;
        FakeResourceProvider resources;
        install(world, resources);

        StaticMeshComponent mesh;
        mesh.meshPath = "mesh/a.obj";
        const Entity entity = createRenderable(world, mesh, materialWithTexture("textures/a.png"));
        update(world, resources);

        const texture_id_type textureId = materialTextureID(world, entity);
        StaticMeshComponent& staticMesh = world.getComponent<StaticMeshComponent>(entity);
        staticMesh.meshPath = "mesh/b.obj";
        gts::rendering::queueStaticMeshRefresh(world, entity);
        update(world, resources);

        return require(materialTextureID(world, entity) == textureId,
                       "mesh change preserves material GPU state")
            && require(resources.meshRequests == 2, "mesh change requests new mesh")
            && require(resources.textureRequests == 1, "mesh change does not request texture again");
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

        world.addComponent(entity, materialWithTexture("textures/a.png"));
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
        const Entity entity = createRenderable(world, mesh, materialWithTexture("textures/a.png"));
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
        world.addComponent(entity, materialWithTexture("textures/a.png"));
        update(world, resources);

        const texture_id_type textureId = materialTextureID(world, entity);
        DynamicMeshComponent& mesh = world.getComponent<DynamicMeshComponent>(entity);
        mesh = triangleMesh(2);
        gts::rendering::queueDynamicMeshRefresh(world, entity);
        update(world, resources);

        return require(materialTextureID(world, entity) == textureId,
                       "dynamic mesh update preserves material")
            && require(resources.proceduralUploads == 2, "dynamic mesh version uploads new geometry")
            && require(resources.textureRequests == 1, "dynamic mesh update does not request material texture again");
    }

    bool geometryDescriptorTypeChangeKeepsObjectSlot()
    {
        ECSWorld world;
        FakeResourceProvider resources;
        install(world, resources);

        StaticMeshComponent mesh;
        mesh.meshPath = "mesh/a.obj";
        const Entity entity = createRenderable(world, mesh, materialWithTexture("textures/a.png"));
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
    ok &= geometryDescriptorTypeChangeKeepsObjectSlot();
    ok &= worldTextUsesMeshAndMaterialLifecycle();

    if (!ok)
        return 1;

    std::printf("RenderLifecycleOwnershipTest passed\n");
    return 0;
}
