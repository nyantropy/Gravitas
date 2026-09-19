#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

#include "ECSWorld.hpp"
#include "GtsScene.hpp"
#include "TransformDirtyHelpers.h"
#include "TransformHierarchyHelpers.h"
#include "TransformInvalidationLifecycle.h"
#include "TransformSceneFeature.h"
#include "TransformSystem.hpp"
#include "WorldTransformComponent.h"

namespace
{
    using namespace gts::transform;

    std::vector<std::string> events;

    void require(bool condition, const char* message)
    {
        if (!condition)
        {
            std::cerr << message << std::endl;
            std::exit(1);
        }
    }

    void published(ECSWorld& world, Entity entity)
    {
        require(world.hasComponent<WorldTransformComponent>(entity), "callback ran before publication");
        events.emplace_back("publish");
    }

    Entity addTransform(ECSWorld& world, float x)
    {
        const Entity       entity = world.createEntity();
        TransformComponent transform;
        transform.position.x = x;
        world.addComponent(entity, transform);
        return entity;
    }

    float position(ECSWorld& world, Entity entity)
    {
        return world.getComponent<WorldTransformComponent>(entity).matrix[3].x;
    }

    size_t transformExecutions(const ECSWorld& world)
    {
        size_t count = 0;
        for (const auto& sample : world.getLastControllerTimingSamples())
        {
            if (sample.name.find("TransformSystem") != std::string_view::npos)
            {
                require(sample.group == EcsSystemGroup::RenderPrep, "resolver group changed");
                require(sample.instanceIndex == count, "unexpected resolver execution index");
                ++count;
            }
        }
        return count;
    }

    class Writer : public ECSControllerSystem
    {
        public:
        explicit Writer(Entity entity) : entity(entity) {}

        void update(const EcsControllerContext& ctx) override
        {
            events.emplace_back("write");
            ctx.world.getComponent<TransformComponent>(entity).position.x = 7.0f;
            markDirty(ctx.world, entity);
        }

        private:
        Entity entity;
    };

    class Reader : public ECSControllerSystem
    {
        public:
        explicit Reader(Entity child) : child(child) {}

        void update(const EcsControllerContext& ctx) override
        {
            events.emplace_back("read");
            require(position(ctx.world, child) == 9.0f, "consumer observed stale child transform");
        }

        private:
        Entity child;
    };

    void completeInstallation()
    {
        ECSWorld world;
        // Installing after authoring must seed the existing transforms too.
        const Entity parent = addTransform(world, 1.0f);
        const Entity child  = addTransform(world, 2.0f);
        installTransformFeature(world);
        require(world.getControllerSystemCount() == 1, "complete installer controller count");
        require(attachToParent(world, child, parent), "could not attach child");
        registerWorldTransformPublishedCallback(world, published);
        registerWorldTransformPublishedCallback(world, published);
        events.clear();
        world.updateControllers(EcsControllerContext{world});
        require(transformExecutions(world) == 1, "complete installer execution count");
        require(events == std::vector<std::string>{"publish", "publish"}, "duplicate publication on clean pass");
        require(position(world, child) == 3.0f, "initial child position");
        require(TransformSystem::getLastMetrics().updatedWorldTransforms == 2, "scheduled pass metrics");
        const auto version = world.getComponent<WorldTransformComponent>(child).version;
        events.clear();
        world.updateControllers(EcsControllerContext{world});
        require(transformExecutions(world) == 1, "clean frame execution count");
        require(events.empty(), "clean frame republished transforms");
        require(world.getComponent<WorldTransformComponent>(child).version == version, "clean frame advanced version");
    }

    void resolverAfterWriter()
    {
        ECSWorld world;
        installTransformRuntime(world);
        require(world.getControllerSystemCount() == 0, "runtime installer must not schedule resolution");
        const Entity parent = addTransform(world, 1.0f);
        const Entity child  = addTransform(world, 2.0f);
        require(attachToParent(world, child, parent), "could not attach child");
        registerWorldTransformPublishedCallback(world, published);
        world.addControllerSystem<Writer>(EcsSystemGroup::Camera, parent);
        installTransformResolver(world);
        world.addControllerSystem<Reader>(EcsSystemGroup::RenderPrep, child);
        events.clear();
        world.updateControllers(EcsControllerContext{world});
        require(transformExecutions(world) == 1, "split installer execution count");
        require(events == std::vector<std::string>{"write", "publish", "publish", "read"},
                "writer/resolver/consumer order");
        require(TransformSystem::getLastMetrics().updatedWorldTransforms == 2, "parent dirty did not propagate");
        events.clear();
        world.updateControllers(EcsControllerContext{world});
        require(events == std::vector<std::string>{"write", "publish", "publish", "read"},
                "subsequent parent-only mutation did not propagate to the child");
    }

    void explicitResolution()
    {
        ECSWorld world;
        installTransformRuntime(world);
        require(world.getControllerSystemCount() == 0, "on-demand runtime scheduled a controller");
        const Entity entity = addTransform(world, 4.0f);
        registerWorldTransformPublishedCallback(world, published);
        events.clear();
        const auto first = TransformWorldResolver{}.resolve(world);
        require(first.updatedWorldTransforms == 1 && position(world, entity) == 4.0f, "on-demand resolution failed");
        require(events == std::vector<std::string>{"publish"}, "on-demand publication failed");
        require(TransformWorldResolver{}.resolve(world).updatedWorldTransforms == 0, "clean on-demand resolution");
        require(world.getLastControllerTimingSamples().empty(), "explicit resolver ran controllers");
    }

    void resolverOnlyAndMasking()
    {
        ECSWorld world;
        installTransformResolver(world);
        require(world.getControllerSystemCount() == 1, "resolver-only registration count");
        const Entity entity = addTransform(world, 5.0f);
        markDirty(world, entity);
        world.pushExecutionProfile(SceneExecutionProfile::pauseMenu());
        world.updateControllers(EcsControllerContext{world});
        require(transformExecutions(world) == 0, "RenderPrep mask ignored");
        require(!world.hasComponent<WorldTransformComponent>(entity), "masked resolver published");
        world.popExecutionProfile();
        world.updateControllers(EcsControllerContext{world});
        require(transformExecutions(world) == 1 && position(world, entity) == 5.0f, "resolver-only execution failed");
    }

    void lateWritersKeepTheirExistingTiming()
    {
        ECSWorld world;
        installTransformFeature(world);
        const Entity entity = addTransform(world, 1.0f);
        world.addControllerSystem<Writer>(EcsSystemGroup::Camera, entity);
        world.updateControllers(EcsControllerContext{world});
        require(position(world, entity) == 1.0f, "installer moved resolution after a later writer");
        world.updateControllers(EcsControllerContext{world});
        require(position(world, entity) == 7.0f, "next frame did not consume late writer");
    }

    class TestScene : public GtsScene
    {
        public:
        void onLoad(EcsControllerContext&, const GtsSceneTransitionData* = nullptr) override {}
        void onUpdateSimulation(const EcsSimulationContext&) override {}
    };

    void sceneInstallation()
    {
        TestScene            scene;
        auto&                world = scene.getWorld();
        EcsControllerContext context{world};
        for (int cycle = 0; cycle != 2; ++cycle)
        {
            installTransformRuntime(scene);
            const Entity parent = addTransform(world, 1.0f);
            const Entity child  = addTransform(world, 2.0f);
            require(attachToParent(world, child, parent), "scene parenting failed");
            world.addControllerSystem<Writer>(EcsSystemGroup::Camera, parent);
            installTransformFeature(scene);
            installTransformFeature(scene);
            installTransformResolver(scene);
            world.addControllerSystem<Reader>(EcsSystemGroup::RenderPrep, child);
            require(world.getControllerSystemCount() == 3, "scene installer idempotence");
            world.updateControllers(context);
            require(transformExecutions(world) == 1, "scene resolver execution count");
            scene.unload(context);
        }
    }
} // namespace

int main()
{
    completeInstallation();
    resolverAfterWriter();
    explicitResolution();
    resolverOnlyAndMasking();
    lateWritersKeepTheirExistingTiming();
    sceneInstallation();
}
