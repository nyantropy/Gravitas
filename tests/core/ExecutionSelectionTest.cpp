#include "ECSWorld.hpp"

#include <functional>
#include <stdexcept>
#include <vector>

namespace
{
    constexpr auto A       = static_cast<EcsSystemGroup>(1ull << 1);
    constexpr auto B       = static_cast<EcsSystemGroup>(1ull << 3);
    constexpr auto Unknown = static_cast<EcsSystemGroup>(1ull << 45);
    void           require(bool value, const char* message)
    {
        if (!value)
            throw std::runtime_error(message);
    }
    EcsExecutionSelection selection(const char* id, EcsSystemMask mask)
    {
        EcsExecutionSelection result;
        result.id             = id;
        result.enabledSystems = mask;
        return result;
    }
    class Controller : public ECSControllerSystem
    {
        public:
        explicit Controller(std::function<void(ECSWorld&)> action) : action(std::move(action)) {}
        void update(const EcsControllerContext& ctx) override
        {
            action(ctx.world);
        }

        private:
        std::function<void(ECSWorld&)> action;
    };
    class Simulation : public ECSSimulationSystem
    {
        public:
        explicit Simulation(std::function<void(ECSWorld&)> action) : action(std::move(action)) {}
        void update(const EcsSimulationContext& ctx) override
        {
            action(ctx.world);
        }

        private:
        std::function<void(ECSWorld&)> action;
    };
    struct Marker
    {
        int value = 7;
    };
} // namespace

int main()
{
    ECSWorld world;
    require(world.configureDefaultExecutionSelection(selection("base", A | B)), "Configure neutral default");
    require(!world.configureDefaultExecutionSelection(selection("replacement", 0)), "Default is installed once");
    std::vector<int> order;
    world.addControllerSystem<Controller>(B,
                                          [&](ECSWorld&)
                                          {
                                              order.push_back(3);
                                          });
    world.addSimulationSystem<Simulation>(B,
                                          [&](ECSWorld& w)
                                          {
                                              order.push_back(1);
                                              auto entity = w.commands().createEntity();
                                              w.commands().addComponent<Marker>(entity, Marker{});
                                          });
    world.addControllerSystem<Controller>(A,
                                          [&](ECSWorld&)
                                          {
                                              order.push_back(4);
                                          });
    world.addSimulationSystem<Simulation>(A,
                                          [&](ECSWorld& w)
                                          {
                                              require(w.hasAny<Marker>(),
                                                      "Structural commands must flush between systems");
                                              order.push_back(2);
                                          });
    world.pushExecutionSelection(selection("both", A | B));
    world.updateSimulation(EcsSimulationContext{world, 0.1f});
    require(order == std::vector<int>{1, 2}, "Simulation list must preserve interleaved registration order");
    world.updateControllers(EcsControllerContext{world});
    require(order == std::vector<int>{1, 2, 3, 4}, "Controllers must remain a separate ordered list");
    const auto samples = world.getLastControllerTimingSamples();
    require(samples.size() == 2 && samples[0].group == B && samples[1].group == A && samples[0].instanceIndex == 0 &&
                samples[1].instanceIndex == 1 && samples[0].name == "Controller",
            "Timing identity/order changed");
    require(!world.popExecutionSelection("wrong") && world.getExecutionSelectionDepth() == 2,
            "Guarded pop must not remove another owner's entry");
    require(world.popExecutionSelection("both") && !world.popExecutionSelection(), "Bottom entry must not be popped");
    world.pushExecutionSelection(selection("none", 0));
    world.updateControllers(EcsControllerContext{world});
    require(world.getLastControllerTimingSamples().empty(), "Zero mask must skip every group");
    require(!world.shouldExecuteGroup(static_cast<EcsSystemGroup>(0)), "Zero group never matches");
    world.pushExecutionSelection(selection("unknown", toMask(Unknown)));
    require(world.shouldExecuteGroup(Unknown) && !world.shouldExecuteGroup(A),
            "Top selection replaces rather than intersects");
    world.pushExecutionSelection(selection("overlap", toMask(A)));
    require(world.shouldExecuteGroup(static_cast<EcsSystemGroup>(A | B)), "Composite group uses any-bit overlap");
    world.clear();
    require(world.getExecutionSelectionDepth() == 1 && world.getControllerSystemCount() == 0 &&
                world.getCurrentExecutionSelection().id == "base" &&
                world.getCurrentExecutionSelection().enabledSystems == (A | B),
            "Clear resets stack and systems");

    world.addControllerSystem<Controller>(A,
                                          [&](ECSWorld& w)
                                          {
                                              order.push_back(5);
                                              w.pushExecutionSelection(selection("mid-pass", toMask(B)));
                                              auto entity = w.commands().createEntity();
                                              w.commands().addComponent<Marker>(entity, Marker{});
                                          });
    world.addControllerSystem<Controller>(A,
                                          [&](ECSWorld&)
                                          {
                                              order.push_back(99);
                                          });
    world.addControllerSystem<Controller>(
        B,
        [&](ECSWorld& w)
        {
            require(w.hasAny<Marker>(), "Controller structural commands must flush before next execution");
            order.push_back(6);
            require(w.popExecutionSelection("mid-pass"), "Mid-pass restoration");
        });
    world.addControllerSystem<Controller>(A,
                                          [&](ECSWorld&)
                                          {
                                              order.push_back(7);
                                          });
    world.pushExecutionSelection(selection("outer", A | B));
    world.updateControllers(EcsControllerContext{world});
    require(order == std::vector<int>{1, 2, 3, 4, 5, 6, 7}, "Mask changes must affect only later systems");
    require(world.getLastControllerTimingSamples()[2].instanceIndex == 2,
            "Skipped systems must not consume timing indices");

    world.clear();
    order.clear();
    world.pushExecutionSelection(selection("outer", A | B));
    world.addSimulationSystem<Simulation>(A,
                                          [&](ECSWorld& w)
                                          {
                                              order.push_back(1);
                                              w.pushExecutionSelection(selection("changed", toMask(B)));
                                          });
    world.addSimulationSystem<Simulation>(A,
                                          [&](ECSWorld&)
                                          {
                                              order.push_back(99);
                                          });
    world.addSimulationSystem<Simulation>(B,
                                          [&](ECSWorld&)
                                          {
                                              order.push_back(2);
                                          });
    world.updateSimulation(EcsSimulationContext{world, 0.1f});
    require(order == std::vector<int>{1, 2}, "Simulation masks must also be read per system");
}
