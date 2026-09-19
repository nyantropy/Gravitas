#include "SceneManager.hpp"

#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace
{
    void require(bool value, const char* message)
    {
        if (!value)
            throw std::runtime_error(message);
    }

    template <typename Action> void requireLogicError(Action action)
    {
        try
        {
            action();
        }
        catch (const std::logic_error&)
        {
            return;
        }
        throw std::runtime_error("Expected an explicit logic_error");
    }

    struct Base
    {
        virtual ~Base() = default;
    };

    struct Resource : Base
    {
        std::vector<std::string>& events;
        int                       value;

        Resource(std::vector<std::string>& events, int value) : events(events), value(value)
        {
            events.push_back("create");
        }
        ~Resource() override
        {
            events.push_back("resource");
        }
    };

    struct SecondResource
    {
        std::vector<std::string>& events;
        ~SecondResource()
        {
            events.push_back("second");
        }
    };

    class Controller : public ECSControllerSystem
    {
        public:
        explicit Controller(std::vector<std::string>& events) : events(events) {}
        ~Controller() override
        {
            events.push_back("world");
        }
        void update(const EcsControllerContext&) override {}

        private:
        std::vector<std::string>& events;
    };

    class Scene : public GtsScene
    {
        public:
        explicit Scene(std::vector<std::string>& events) : events(events) {}
        using GtsScene::resetSceneWorld;
        void onLoad(EcsControllerContext&, const GtsSceneTransitionData*) override
        {
            install();
        }
        void onUpdateSimulation(const EcsSimulationContext&) override {}
        void onUnload(EcsControllerContext&) override
        {
            require(findSceneResource<Resource>() != nullptr, "Unload must precede resource destruction");
            events.push_back("unload");
        }
        void install()
        {
            if (!markSceneFeatureInstalled("test"))
                return;
            createSceneResource<Resource>(events, 42);
            createSceneResource<SecondResource>(events);
            getWorld().addControllerSystem<Controller>(static_cast<EcsSystemGroup>(1ull), events);
            registerSceneResetHook(
                [this](ECSWorld& world)
                {
                    require(findSceneResource<Resource>() && world.getControllerSystemCount() == 1,
                            "Reset hook must precede world/resource destruction");
                    events.push_back("hook");
                });
        }

        private:
        std::vector<std::string>& events;
    };

    static_assert(std::is_same_v<decltype(std::declval<Scene&>().findSceneResource<Resource>()), Resource*>);
    static_assert(
        std::is_same_v<decltype(std::declval<const Scene&>().findSceneResource<Resource>()), const Resource*>);
    static_assert(std::is_same_v<decltype(std::declval<Scene&>().requireSceneResource<Resource>()), Resource&>);
    static_assert(
        std::is_same_v<decltype(std::declval<const Scene&>().requireSceneResource<Resource>()), const Resource&>);
} // namespace

int main()
{
    std::vector<std::string> events;
    Scene                    scene(events);
    require(scene.findSceneResource<Resource>() == nullptr, "Missing lookup must not create");
    require(std::as_const(scene).findSceneResource<Resource>() == nullptr, "Const missing lookup");
    requireLogicError(
        [&]
        {
            scene.requireSceneResource<Resource>();
        });
    requireLogicError(
        [&]
        {
            std::as_const(scene).requireSceneResource<Resource>();
        });

    for (int cycle = 0; cycle < 3; ++cycle)
    {
        scene.install();
        Resource* resource = scene.findSceneResource<Resource>();
        scene.install();
        require(scene.getWorld().getControllerSystemCount() == 1, "Repeated install must remain idempotent");
        require(resource == &scene.requireSceneResource<Resource>(), "Lookup must return the owned object");
        resource->value = 17;
        require(std::as_const(scene).requireSceneResource<Resource>().value == 17, "Const lookup sees same resource");
        require(std::as_const(scene).findSceneResource<Resource>() == resource, "Const pointer identity");
        require(scene.findSceneResource<Base>() == nullptr, "Lookup must use exact type, not inheritance");
        const auto beforeDuplicate = events.size();
        requireLogicError(
            [&]
            {
                scene.createSceneResource<Resource>(events, 99);
            });
        require(events.size() == beforeDuplicate && resource->value == 17, "Duplicate must not construct or replace");
        events.clear();
        EcsControllerContext ctx{scene.getWorld()};
        scene.unload(ctx);
        require(events == std::vector<std::string>{"unload", "hook", "world", "resource", "second"},
                "Unload/reset destruction ordering changed");
        require(!scene.findSceneResource<Resource>() && !scene.findSceneResource<SecondResource>(),
                "Reset must clear all lookups");
        events.clear();
        scene.resetSceneWorld();
        require(events.empty(), "Repeated reset must clear hooks and resources");
    }

    std::vector<std::string> otherEvents;
    auto                     other = std::make_unique<Scene>(otherEvents);
    scene.install();
    other->install();
    require(scene.findSceneResource<Resource>() != other->findSceneResource<Resource>(),
            "Scene resources must be isolated");
    events.clear();
    otherEvents.clear();
    other.reset();
    require(otherEvents == std::vector<std::string>{"resource", "second", "world"},
            "Direct destruction is not unload/reset");
    require(events.empty() && scene.findSceneResource<Resource>(),
            "Destroying another scene must not affect resources");

    scene.getWorld().clear();
    require(scene.findSceneResource<Resource>() && !scene.markSceneFeatureInstalled("test"),
            "World clear must not reset scene resources or installation bookkeeping");
    // This scene's reset hook expects its controller; direct destruction below
    // deliberately does not invoke that hook after a raw world clear.

    std::vector<std::string> transitions;
    SceneManager             manager;
    manager.registerScene("scene",
                          [&]
                          {
                              return std::make_unique<Scene>(transitions);
                          });
    manager.setActiveScene("scene");
    {
        auto&                active = *manager.getActiveScene();
        EcsControllerContext ctx{active.getWorld()};
        active.onLoad(ctx);
        transitions.clear();
        active.unload(ctx);
    }
    manager.clearActiveScene();
    require(transitions == std::vector<std::string>{"unload", "hook", "world", "resource", "second"},
            "Normal scene transition must unload once before destruction");
    require(manager.getActiveScene() == nullptr && manager.getActiveSceneName().empty(), "Scene manager clear");
    manager.setActiveScene("scene");
    {
        auto&                active = *manager.getActiveScene();
        EcsControllerContext ctx{active.getWorld()};
        active.onLoad(ctx);
    }
    transitions.clear();
    manager.setActiveScene("scene");
    require(transitions == std::vector<std::string>{"resource", "second", "world"},
            "Factory replacement must retain direct-destruction semantics");
}
