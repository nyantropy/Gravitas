#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <vector>

#include "InputBindingRegistry.h"

class TestInput : public IInputSource
{
public:
    bool down = true;
    bool pressed = true;
    bool released = false;
    bool isKeyDown(GtsKey key) const override { return key == GtsKey::W && down; }
    bool isKeyPressed(GtsKey key) const override { return key == GtsKey::W && pressed; }
    bool isKeyReleased(GtsKey key) const override { return key == GtsKey::W && released; }
    bool isMouseButtonDown(int) const override { return false; }
    bool isMouseButtonPressed(int) const override { return false; }
    bool isMouseButtonReleased(int) const override { return false; }
    double mouseX() const override { return 0; }
    double mouseY() const override { return 0; }
    double scrollX() const override { return 0; }
    double scrollY() const override { return 0; }
    ModifierFlags getModifiers() const override { return ModifierFlags::None; }
};

InputBinding binding(const char* action, const char* context, bool passthrough = false,
                     PausePolicy policy = PausePolicy::AlwaysActive,
                     ActivationMode mode = ActivationMode::Pressed)
{
    return {action, {InputTrigger::Type::Key, static_cast<int>(GtsKey::W)},
            mode, context, policy, passthrough};
}

void require(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}

int main()
{
    try
    {
        for (bool reverse : {false, true})
        {
            TestInput raw;
            InputBindingRegistry registry;
            auto load = [&](std::vector<InputBinding> bindings)
            {
                registry.clearAllBindings();
                registry.clearContextStack();
                registry.setPaused(false);
                if (reverse) std::reverse(bindings.begin(), bindings.end());
                registry.loadBindings(bindings);
            };

            load({binding("observer", "top", true), binding("owner", "top"),
                  binding("lower", "", false, PausePolicy::AlwaysActive, ActivationMode::Held)});
            registry.pushContext("top");
            registry.update(InputSnapshot{&raw});
            require(registry.isPressed("observer") && registry.isPressed("owner"), "Sibling sharing depends on order");
            require(!registry.isPressed("lower"), "Exclusive context failed to block lower context");
            require(registry.getRoutingConflicts().empty(), "Observer incorrectly conflicts");
            require(registry.isSimulationPressed("owner"), "Simulation edge missing");
            raw.pressed = false;
            registry.update(InputSnapshot{&raw});
            require(registry.isSimulationPressed("owner"), "Simulation edge lost before tick");
            registry.finishSimulationTick();
            require(!registry.isSimulationPressed("owner"), "Simulation edge repeated after tick");
            require(!registry.isHeld("lower"), "Pressed binding lost ownership while key held");
            raw.pressed = true;

            load({binding("a", "top"), binding("b", "top"), binding("observer", "top", true),
                  binding("lower", "")});
            registry.pushContext("top");
            registry.update(InputSnapshot{&raw});
            require(!registry.isPressed("a") && !registry.isPressed("b"), "Conflict selected a winner");
            require(registry.isPressed("observer") && !registry.isPressed("lower"), "Conflict routing incorrect");
            const auto& conflicts = registry.getRoutingConflicts();
            require(conflicts.size() == 1 && conflicts[0].context == "top"
                    && conflicts[0].actions == std::vector<std::string>{"a", "b"}, "Conflict diagnostic missing or unordered");

            load({binding("gameplay", "top", false, PausePolicy::Gameplay), binding("always", "")});
            registry.pushContext("top");
            registry.setPaused(true);
            registry.update(InputSnapshot{&raw});
            require(!registry.isPressed("gameplay") && registry.isPressed("always"), "Paused binding reserves input");
            require(!registry.isSimulationPressed("gameplay"), "Paused binding queued simulation input");

            load({binding("observer", "top", true), binding("lower", "")});
            registry.pushContext("top");
            registry.update(InputSnapshot{&raw});
            require(registry.isPressed("observer") && registry.isPressed("lower"), "Passthrough blocks lower context");

            load({binding("gameplay", "", false, PausePolicy::Gameplay), binding("always", "")});
            registry.setPaused(true);
            registry.update(InputSnapshot{&raw});
            require(registry.isPressed("always") && registry.getRoutingConflicts().empty(), "Paused sibling creates conflict");

            load({binding("low", "low"), binding("high", "high")});
            registry.pushContext("low");
            registry.pushContext("high");
            registry.update(InputSnapshot{&raw});
            require(registry.isPressed("high") && !registry.isPressed("low"), "Context precedence changed");
            registry.popContext("high");
            registry.update(InputSnapshot{&raw});
            require(registry.isPressed("low"), "Context removal failed");

            load({binding("press", "", false, PausePolicy::AlwaysActive, ActivationMode::Pressed),
                  binding("press", "", false, PausePolicy::AlwaysActive, ActivationMode::Released)});
            raw.down = false;
            raw.pressed = false;
            raw.released = true;
            registry.update(InputSnapshot{&raw});
            require(registry.getRoutingConflicts().empty(), "Same action conflicts with itself");
            require(registry.isSimulationReleased("press"), "Release edge missing");
            registry.finishSimulationTick();
            require(!registry.isSimulationReleased("press"), "Release edge not consumed");
        }
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
