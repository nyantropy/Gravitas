#include <cstdlib>
#include <iostream>
#include <memory>
#include <new>
#include <utility>
#include <vector>

#include "ECSWorld.hpp"
#include "GtsScene.hpp"
#include "TransformDirtyHelpers.h"
#include "TransformHierarchyHelpers.h"
#include "TransformInvalidationLifecycle.h"
#include "TransformSceneFeature.h"
#include "TransformWorldResolver.h"
#include "detail/TransformInvalidationState.h"

namespace
{
    using namespace gts::transform;

    // Constructed before the function-static registries: tests shutdown ordering.
    ECSWorld                                     shutdownWorld;
    std::vector<std::pair<const ECSWorld*, int>> deliveries;

    void require(bool condition, const char* message)
    {
        if (!condition)
        {
            std::cerr << message << std::endl;
            std::exit(1);
        }
    }

    void firstCallback(ECSWorld& world, Entity)
    {
        deliveries.emplace_back(&world, 1);
    }

    void secondCallback(ECSWorld& world, Entity)
    {
        deliveries.emplace_back(&world, 2);
    }

    void requireReleased(const ECSWorld* world)
    {
        const auto state = inspectTransformWorldState(world);
        require(!state.hasInvalidation && !state.hasPublication, "world still retained in transform registries");
    }

    Entity addTransform(ECSWorld& world)
    {
        Entity entity = world.createEntity();
        world.addComponent(entity, TransformComponent{});
        markDirty(world, entity);
        return entity;
    }

    void installAndReset()
    {
        ECSWorld world;
        installTransformFeature(world);
        require(world.getControllerSystemCount() == 2, "combined installation changed resolver scheduling");
        require(world.getEntityCount() == 0, "lifetime installation created an ECS entity");
        auto state = inspectTransformWorldState(&world);
        require(state.hasInvalidation && state.hasPublication, "installation did not establish both registries");
        registerWorldTransformPublishedCallback(world, firstCallback);
        registerWorldTransformPublishedCallback(world, secondCallback);
        registerWorldTransformPublishedCallback(world, firstCallback);
        registerWorldTransformPublishedCallback(world, nullptr);
        addTransform(world);
        state = inspectTransformWorldState(&world);
        require(state.queuedEntities == 1 && state.dirtyFlags > 0 && state.callbacks == 2,
                "dirty state or callback deduplication changed");
        deliveries.clear();
        TransformWorldResolver resolver;
        resolver.resolve(world);
        require(deliveries == std::vector<std::pair<const ECSWorld*, int>>{{&world, 1}, {&world, 2}},
                "publication order/delivery changed");
        resetTransformSceneFeature(world);
        requireReleased(&world);
        require(world.getControllerSystemCount() == 2, "runtime state reset changed registered systems");

        // A later dirty operation may reestablish state, but not old callbacks.
        deliveries.clear();
        markDirty(world, Entity{0});
        resolver.resolve(world);
        require(deliveries.empty(), "reset retained an old callback");
        world.clear();
        requireReleased(&world);
    }

    void independentWorlds()
    {
        auto     first = std::make_unique<ECSWorld>();
        ECSWorld second;
        installTransformFeature(*first);
        installTransformFeature(second);
        registerWorldTransformPublishedCallback(*first, firstCallback);
        registerWorldTransformPublishedCallback(second, secondCallback);
        addTransform(*first);
        addTransform(second);
        const ECSWorld* oldAddress = first.get();
        first.reset();
        requireReleased(oldAddress);
        auto remaining = inspectTransformWorldState(&second);
        require(remaining.queuedEntities == 1 && remaining.callbacks == 1, "destroying one world modified another");
        deliveries.clear();
        TransformWorldResolver{}.resolve(second);
        require(deliveries == std::vector<std::pair<const ECSWorld*, int>>{{&second, 2}},
                "surviving world's callback was lost");
        second.clear();
        requireReleased(&second);
    }

    void repeatedClearAndRemoval()
    {
        ECSWorld world;
        for (int cycle = 0; cycle != 5; ++cycle)
        {
            installTransformFeature(world);
            registerWorldTransformPublishedCallback(world, firstCallback);
            Entity parent = addTransform(world);
            Entity child  = addTransform(world);
            require(attachToParent(world, child, parent), "hierarchy setup failed");
            TransformWorldResolver{}.resolve(world);
            require(inspectTransformWorldState(&world).queuedEntities == 0, "resolve did not drain dirty state");
            // Hierarchy removal during clear queues fresh dirty work. Cleanup must run afterward.
            world.clear();
            requireReleased(&world);
            require(world.getControllerSystemCount() == 0, "world clear retained systems");
        }
    }

    void addressReuseAndLazyState()
    {
        alignas(ECSWorld) std::byte storage[sizeof(ECSWorld)];
        ECSWorld*                   world = std::construct_at(reinterpret_cast<ECSWorld*>(storage));
        // Callback-only use must also acquire lifetime protection.
        registerWorldTransformPublishedCallback(*world, firstCallback);
        require(inspectTransformWorldState(world).callbacks == 1, "callback-only installation failed");
        std::destroy_at(world);
        requireReleased(world);

        world = std::construct_at(reinterpret_cast<ECSWorld*>(storage));
        // Dirty-only use before installing any systems must be cleaned as well.
        addTransform(*world);
        deliveries.clear();
        TransformWorldResolver{}.resolve(*world);
        require(deliveries.empty(), "reused address observed the previous world's callback");
        queueTransformDirty(*world, Entity{1000});
        std::destroy_at(world);
        requireReleased(world);

        world = std::construct_at(reinterpret_cast<ECSWorld*>(storage));
        installTransformResolver(*world);
        require(world->getControllerSystemCount() == 1, "resolver-only installation changed scheduling");
        auto result = TransformWorldResolver{}.resolve(*world);
        require(result.queuedTransforms == 0, "reused address retained dirty entities");
        std::destroy_at(world);
        requireReleased(world);
    }

    void repeatedExplicitReset()
    {
        ECSWorld world;
        for (size_t cycle = 0; cycle != 5; ++cycle)
        {
            installTransformRuntime(world);
            registerWorldTransformPublishedCallback(world, firstCallback);
            addTransform(world);
            resetTransformSceneFeature(world);
            resetTransformSceneFeature(world);
            requireReleased(&world);
            require(world.getControllerSystemCount() == cycle + 1,
                    "explicit state reset removed or deduplicated controllers");
        }
        queueTransformDirty(world, Entity{0});
        world.clear();
        requireReleased(&world);
    }

    class TestScene : public GtsScene
    {
        public:
        void onLoad(EcsControllerContext&, const GtsSceneTransitionData* = nullptr) override {}
        void onUpdateSimulation(const EcsSimulationContext&) override {}
    };

    void sceneLifetime()
    {
        const ECSWorld* address = nullptr;
        {
            TestScene scene;
            ECSWorld& world = scene.getWorld();
            address         = &world;
            EcsControllerContext frame{world};
            for (int cycle = 0; cycle != 3; ++cycle)
            {
                installTransformFeature(scene);
                installTransformFeature(scene);
                require(world.getControllerSystemCount() == 2,
                        "scene idempotence or duplicate-registration behavior changed");
                registerWorldTransformPublishedCallback(world, firstCallback);
                const Entity parent = addTransform(world);
                const Entity child  = addTransform(world);
                attachToParent(world, child, parent);
                scene.unload(frame);
                requireReleased(&world);
            }
            installTransformFeature(scene);
            registerWorldTransformPublishedCallback(world, firstCallback);
            addTransform(world);
            // Destruction without unload must release transform state too.
        }
        requireReleased(address);
    }
} // namespace

int main()
{
    installAndReset();
    independentWorlds();
    repeatedClearAndRemoval();
    addressReuseAndLazyState();
    repeatedExplicitReset();
    sceneLifetime();
    const auto state = inspectTransformWorldState();
    require(state.invalidationWorlds == 0 && state.publicationWorlds == 0, "test worlds leaked registry entries");
    installTransformRuntime(shutdownWorld);
    registerWorldTransformPublishedCallback(shutdownWorld, firstCallback);
}
