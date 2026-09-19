#include "SceneExecutionPolicy.h"
#include "RendererSceneFeature.h"
#include "RenderingBenchmark.h"
#include "AssetPreviewWorld.hpp"
#include "ParticlePreviewWorld.hpp"
#include "material/TestMaterialResources.h"

#include <array>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string_view>

namespace
{
    using namespace gts::rendering;

    ECSWorld                        staticWorld;
    RenderExtractionSnapshotBuilder staticBuilder;

    void require(bool condition, const char* message)
    {
        if (!condition)
            throw std::runtime_error(message);
    }

    using Counts = std::array<size_t, 5>;
    Counts counts()
    {
        return {materialRuntimeRegistry().size(),
                sharedUnlitMaterialCacheRegistry().size(),
                geometryBindingLifecycleRegistry().size(),
                renderInvalidationRegistry().size(),
                cameraBindingLifecycleRegistry().size()};
    }

    void requireAbsent(ECSWorld* world)
    {
        require(!materialRuntimeRegistry().contains(world), "material runtime retained world");
        require(!sharedUnlitMaterialCacheRegistry().contains(world), "shared material cache retained world");
        require(!geometryBindingLifecycleRegistry().contains(world), "geometry binding retained world");
        require(!renderInvalidationRegistry().contains(world), "render invalidation retained world");
        require(!cameraBindingLifecycleRegistry().contains(world), "camera binding retained world");
    }

    void populate(ECSWorld& world)
    {
        getOrCreateSharedUnlitMaterial(world, {});
        queueRenderObjectRefresh(world, Entity{17});
        queueRenderSnapshotDirty(world, Entity{17});
        queueCameraCleanup(world, Entity{17});
    }

    struct TrackedResources : Resources
    {
        int                      objects = 0;
        int                      meshes  = 0;
        int                      cameras = 0;
        std::weak_ptr<const int> materialLifetime;
        void                     check()
        {
            require(!materialLifetime.expired(), "material state erased before resource removal");
        }
        void releaseObjectSlot(ssbo_id_type) override
        {
            check();
            ++objects;
        }
        void releaseProceduralMesh(mesh_id_type) override
        {
            check();
            ++meshes;
        }
        void releaseCameraBuffer(view_id_type) override
        {
            check();
            ++cameras;
        }
    };

    void install(ECSWorld& world, TrackedResources& resources)
    {
        const auto execution = gts::execution::rendererExecutionInputs();
        installRendererGeometrySceneFeature(world, &resources, execution);
        installRendererCameraSceneFeature(world, &resources, execution);
        populate(world);
        resources.materialLifetime = materialRuntime(world).lifetimeToken();
    }

    void addGpuState(ECSWorld& world)
    {
        const Entity       entity = world.createEntity();
        RenderGpuComponent render;
        render.objectSSBOSlot = 7;
        world.addComponent(entity, render);
        MeshGpuComponent mesh;
        mesh.meshID                     = 8;
        mesh.ownsProceduralMeshResource = true;
        world.addComponent(entity, mesh);
        CameraGpuComponent camera;
        camera.viewID = 9;
        world.addComponent(entity, camera);
        world.addComponent(entity, StaticMeshComponent{});
        world.addComponent(entity, CameraDescriptionComponent{});
        world.addComponent(entity, MaterialReferenceComponent{materialRuntime(world).defaultMaterial()});
    }

    void characterizeRemoval()
    {
        ECSWorld         world;
        TrackedResources resources;
        install(world, resources);
        addGpuState(world);
        world.clear();
        require(resources.objects == 1 && resources.meshes == 1 && resources.cameras == 1,
                "clear must release each GPU resource exactly once through removal callbacks");
        world.clear();
        require(resources.objects == 1 && resources.meshes == 1 && resources.cameras == 1,
                "second clear must not release resources twice");
        // Existing explicit resets let the characterization run on the pre-fix implementation too.
        resetRendererGeometrySceneFeature(world);
        resetRendererCameraSceneFeature(world);
    }

    void individualStatesAndLateCreation()
    {
        const auto baseline = counts();
        for (int state = 0; state != 5; ++state)
        {
            ECSWorld world;
            switch (state)
            {
            case 0:
                materialRuntime(world);
                break;
            case 1:
                getOrCreateSharedUnlitMaterial(world, {});
                break;
            case 2:
                queueRenderObjectRefresh(world, Entity{1});
                break;
            case 3:
                queueRenderSnapshotDirty(world, Entity{1});
                break;
            case 4:
                queueCameraCleanup(world, Entity{1});
                break;
            }
            world.clear();
            requireAbsent(&world);
        }
        struct Probe
        {
        };
        ECSWorld world;
        world.registerRemoveCallback<Probe>(
            [](ECSWorld& removingWorld, Entity, Probe&)
            {
                populate(removingWorld);
            });
        world.addComponent(world.createEntity(), Probe{});
        require(counts() == baseline, "probe must not create state before removal");
        world.clear();
        requireAbsent(&world);
        require(counts() == baseline, "state first created by removal must be released in that clear");
    }

    void clearAndReinstall()
    {
        ECSWorld         world;
        TrackedResources resources;
        for (int cycle = 0; cycle != 3; ++cycle)
        {
            install(world, resources);
            addGpuState(world);
            world.clear();
            requireAbsent(&world);
            require(resources.materialLifetime.expired(), "clear must expire material lifetime");
        }
        install(world, resources);
        resetRendererGeometrySceneFeature(world);
        resetRendererCameraSceneFeature(world);
        populate(world);
        world.clear();
        requireAbsent(&world);
    }

    struct Scene : GtsScene
    {
        void onLoad(EcsControllerContext&, const GtsSceneTransitionData*) override {}
        void onUpdateSimulation(const EcsSimulationContext&) override {}
    };

    void sceneLifecycle()
    {
        Scene scene;
        for (int cycle = 0; cycle != 3; ++cycle)
        {
            auto&                resources = scene.createSceneResource<TrackedResources>();
            EcsControllerContext context{scene.getWorld()};
            controllerContext(context).resources = &resources;
            installRendererFeature(scene, context, gts::execution::rendererExecutionInputs());
            const auto systemCount = scene.getWorld().getControllerSystemCount();
            installRendererFeature(scene, context, gts::execution::rendererExecutionInputs());
            require(scene.getWorld().getControllerSystemCount() == systemCount, "scene installation remains guarded");
            populate(scene.getWorld());
            resources.materialLifetime = materialRuntime(scene.getWorld()).lifetimeToken();
            const auto token           = resources.materialLifetime;
            addGpuState(scene.getWorld());
            scene.registerSceneResetHook(
                [&resources](ECSWorld&)
                {
                    resources.check();
                });
            scene.unload(context);
            requireAbsent(&scene.getWorld());
            require(token.expired(), "unload must release materials");
            require(scene.findSceneResource<TrackedResources>() == nullptr, "scene resources reset unchanged");
        }
        ECSWorld*                address = nullptr;
        std::weak_ptr<const int> token;
        {
            Scene direct;
            address         = &direct.getWorld();
            auto& resources = direct.createSceneResource<TrackedResources>();
            install(direct.getWorld(), resources);
            addGpuState(direct.getWorld());
            token = resources.materialLifetime;
        }
        requireAbsent(address);
        require(token.expired(), "direct scene destruction must release CPU state without provider access");
    }

    void isolatedWorldsAndAddressReuse()
    {
        ECSWorld survivor;
        populate(survivor);
        const auto                  survivorToken = materialRuntime(survivor).lifetimeToken();
        alignas(ECSWorld) std::byte storage[sizeof(ECSWorld)];
        for (int cycle = 0; cycle != 3; ++cycle)
        {
            auto* world = std::construct_at(reinterpret_cast<ECSWorld*>(storage));
            requireAbsent(world);
            populate(*world);
            auto token = materialRuntime(*world).lifetimeToken();
            std::destroy_at(world);
            requireAbsent(world);
            require(token.expired(), "destroyed world must expire its material lifetime");
            require(!survivorToken.expired(), "destroying another world must not affect survivor");
            require(geometryBindingLifecycleState(survivor).renderObjectRefreshEntities.contains(17),
                    "survivor queues remain intact");
        }
        survivor.clear();
        requireAbsent(&survivor);
    }

    void snapshotWorldFirst()
    {
        ECSWorld                        otherWorld;
        RenderExtractionSnapshotBuilder otherBuilder;
        const auto                      otherVersion = otherBuilder.build(otherWorld).contentVersion;
        RenderExtractionSnapshotBuilder builder;
        uint64_t                        generation = 0;
        alignas(ECSWorld) std::byte     storage[sizeof(ECSWorld)];
        for (int cycle = 0; cycle != 3; ++cycle)
        {
            auto* world = std::construct_at(reinterpret_cast<ECSWorld*>(storage));
            require(builder.build(*world).contentVersion > generation, "new world must publish a fresh generation");
            generation = builder.getLatestSnapshot().contentVersion;
            world->clear();
            require(builder.getLatestSnapshot().contentVersion == 0, "world clear must detach surviving builder");
            require(builder.build(*world).contentVersion > generation, "builder reattachment advances generation");
            generation = builder.getLatestSnapshot().contentVersion;
            std::destroy_at(world);
            require(builder.getLatestSnapshot().contentVersion == 0, "world destruction must detach surviving builder");
            require(otherBuilder.getLatestSnapshot().contentVersion == otherVersion, "other builder remains intact");
        }
        otherBuilder.resetSceneState();
        {
            RenderExtractionSnapshotBuilder shortLived;
            shortLived.build(otherWorld);
            shortLived.resetSceneState();
            shortLived.build(otherWorld);
        }
        otherWorld.clear();

        RenderCommandExtractor extractor;
        auto&                  snapshot = builder.build(otherWorld);
        RenderableSnapshot     renderable;
        renderable.objectSSBOSlot = 3;
        renderable.meshID         = 42;
        renderable.visible        = true;
        snapshot.renderables.push_back(renderable);
        require(extractor.extract(snapshot).size() == 1, "extractor has a cached command");
        otherWorld.clear();
        require(extractor.extract(builder.build(otherWorld)).empty(),
                "reattachment must invalidate old extraction commands");
    }

    void providerCanDieBeforeWorld()
    {
        auto                     world = std::make_unique<ECSWorld>();
        std::weak_ptr<const int> token;
        {
            TrackedResources resources;
            install(*world, resources);
            addGpuState(*world);
            token = resources.materialLifetime;
        }
        auto* address = world.get();
        world.reset();
        requireAbsent(address);
        require(token.expired(), "identity-only destruction must not need a live provider");
    }

    void previewsAndBenchmarks()
    {
        Resources  resources;
        const auto baseline = counts();
        {
            gts::tools::AssetPreviewWorld    assets(gts::execution::rendererExecutionInputs());
            gts::tools::ParticlePreviewWorld particles(gts::execution::rendererExecutionInputs());
            for (int cycle = 0; cycle != 3; ++cycle)
            {
                assets.ensure(&resources);
                particles.ensure(&resources);
                populate(particles.ecsWorld());
                assets.destroy();
                require(materialRuntimeRegistry().contains(&particles.ecsWorld()), "other preview survives");
                particles.destroy();
                requireAbsent(&particles.ecsWorld());
            }
            assets.ensure(&resources);
            particles.ensure(&resources);
            populate(particles.ecsWorld());
        }
        require(counts() == baseline, "preview abandon/destruction must release registries");
        auto config                   = benchmarks::findBenchmarkPreset("static_geometry_small")->config;
        config.renderableCount        = 4;
        config.visibleRenderableCount = 4;
        config.warmupFrames           = 1;
        config.measuredFrames         = 2;
        for (int cycle = 0; cycle != 3; ++cycle)
        {
            const auto result = benchmarks::runRenderingBenchmark(
                config, {gts::execution::rendererExecutionInputs(), ecsSystemGroupName});
            require(result.invariantFailures.empty(), "benchmark invariants unchanged");
            require(counts() == baseline, "private benchmark world must release registries");
        }
    }
} // namespace

int main(int argc, char** argv)
{
    try
    {
        if (argc == 2 && std::string_view(argv[1]) == "--static-shutdown")
        {
            populate(staticWorld);
            staticBuilder.build(staticWorld);
            return 0;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--snapshot")
        {
            snapshotWorldFirst();
            return 0;
        }
        characterizeRemoval();
        if (argc == 2 && std::string_view(argv[1]) == "--characterize")
        {
            std::cout << "Rendering removal characterization passed\n";
            return 0;
        }
        individualStatesAndLateCreation();
        clearAndReinstall();
        sceneLifecycle();
        isolatedWorldsAndAddressReuse();
        snapshotWorldFirst();
        providerCanDieBeforeWorld();
        previewsAndBenchmarks();
        require(counts() == Counts{}, "all rendering registries must be empty");
        std::cout << "Renderer world lifetime passed\n";
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
