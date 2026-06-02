#include "InputBindingRegistry.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

#include "GtsKey.h"

namespace
{
    template<typename T>
    void hashCombine(size_t& seed, const T& value)
    {
        seed ^= std::hash<T>{}(value) + 0x9e3779b9u + (seed << 6u) + (seed >> 2u);
    }

    bool isModifierKey(GtsKey key)
    {
        switch (key)
        {
            case GtsKey::LeftShift:
            case GtsKey::RightShift:
            case GtsKey::LeftCtrl:
            case GtsKey::RightCtrl:
            case GtsKey::LeftAlt:
            case GtsKey::RightAlt:
            case GtsKey::LeftSuper:
            case GtsKey::RightSuper:
                return true;
            default:
                return false;
        }
    }

    ModifierFlags modifierFlagForKey(GtsKey key)
    {
        switch (key)
        {
            case GtsKey::LeftShift:
            case GtsKey::RightShift:
                return ModifierFlags::Shift;
            case GtsKey::LeftCtrl:
            case GtsKey::RightCtrl:
                return ModifierFlags::Ctrl;
            case GtsKey::LeftAlt:
            case GtsKey::RightAlt:
                return ModifierFlags::Alt;
            case GtsKey::LeftSuper:
            case GtsKey::RightSuper:
                return ModifierFlags::Super;
            default:
                return ModifierFlags::None;
        }
    }
}

bool InputTrigger::operator==(const InputTrigger& other) const
{
    return type == other.type
        && code == other.code
        && modifiers == other.modifiers
        && axisIndex == other.axisIndex
        && std::fabs(deadzone - other.deadzone) < 0.0001f
        && std::fabs(axisDirection - other.axisDirection) < 0.0001f;
}

size_t InputTrigger::hash() const
{
    size_t seed = 0;
    hashCombine(seed, static_cast<int>(type));
    hashCombine(seed, code);
    hashCombine(seed, static_cast<int>(modifiers));
    hashCombine(seed, axisIndex);
    hashCombine(seed, static_cast<int>(deadzone * 10000.0f));
    hashCombine(seed, static_cast<int>(axisDirection * 10000.0f));
    return seed;
}

bool InputBinding::operator==(const InputBinding& other) const
{
    return action == other.action
        && trigger == other.trigger
        && mode == other.mode
        && context == other.context
        && pausePolicy == other.pausePolicy
        && passthrough == other.passthrough;
}

bool InputBindingRegistry::ConsumedInput::operator==(const ConsumedInput& other) const
{
    return type == other.type
        && code == other.code
        && axisIndex == other.axisIndex
        && std::fabs(axisDirection - other.axisDirection) < 0.0001f;
}

size_t InputBindingRegistry::ConsumedInput::hash() const
{
    size_t seed = 0;
    hashCombine(seed, static_cast<int>(type));
    hashCombine(seed, code);
    hashCombine(seed, axisIndex);
    hashCombine(seed, static_cast<int>(axisDirection * 10000.0f));
    return seed;
}

void InputBindingRegistry::bind(const InputBinding& binding)
{
    if (binding.action.empty())
        return;

    const auto existing = std::find(bindings.begin(), bindings.end(), binding);
    if (existing != bindings.end())
        return;

    bindings.push_back(binding);
    ensureActionState(binding.action);
    rebuildActionIndex();
    clearSimulationEdges();
}

void InputBindingRegistry::bind(const std::string& action, InputTrigger trigger,
                                ActivationMode mode,
                                const std::string& context,
                                PausePolicy pausePolicy)
{
    bind(InputBinding{action, trigger, mode, context, pausePolicy, false});
}

void InputBindingRegistry::unbind(const std::string& action, const InputTrigger& trigger)
{
    bindings.erase(std::remove_if(bindings.begin(),
                                  bindings.end(),
                                  [&](const InputBinding& binding)
                                  {
                                      return binding.action == action && binding.trigger == trigger;
                                  }),
                   bindings.end());
    rebuildActionIndex();
    clearSimulationEdges();
}

void InputBindingRegistry::unbindAll(const std::string& action)
{
    bindings.erase(std::remove_if(bindings.begin(),
                                  bindings.end(),
                                  [&](const InputBinding& binding)
                                  {
                                      return binding.action == action;
                                  }),
                   bindings.end());
    rebuildActionIndex();
    clearSimulationEdges();
}

void InputBindingRegistry::rebind(const std::string& action,
                                  const InputTrigger& oldTrigger,
                                  const InputTrigger& newTrigger)
{
    for (auto& binding : bindings)
    {
        if (binding.action == action && binding.trigger == oldTrigger)
        {
            binding.trigger = newTrigger;
        }
    }
    rebuildActionIndex();
    clearSimulationEdges();
}

void InputBindingRegistry::loadBindings(const std::vector<InputBinding>& newBindings)
{
    bindings = newBindings;
    rebuildActionIndex();
    clearSimulationEdges();
}

std::vector<InputBinding> InputBindingRegistry::exportBindings() const
{
    return bindings;
}

void InputBindingRegistry::clearAllBindings()
{
    bindings.clear();
    bindingsByAction.clear();
    clearActionRuntimeState();
}

void InputBindingRegistry::pushContext(const std::string& context)
{
    if (context.empty())
        return;

    pendingContextOps.push_back({PendingContextOpType::Push, context});
}

void InputBindingRegistry::popContext(const std::string& context)
{
    if (context.empty())
        return;

    pendingContextOps.push_back({PendingContextOpType::Pop, context});
}

void InputBindingRegistry::clearContextStack()
{
    pendingContextOps.push_back({PendingContextOpType::Clear, {}});
}

bool InputBindingRegistry::isContextActive(const std::string& context) const
{
    if (context.empty())
        return true;

    return std::find(activeContexts.begin(), activeContexts.end(), context) != activeContexts.end();
}

bool InputBindingRegistry::isPressed(const std::string& action) const
{
    if (paused)
        return policyPressed(action, PausePolicy::AlwaysActive);

    return policyPressed(action, PausePolicy::Gameplay)
        || policyPressed(action, PausePolicy::AlwaysActive);
}

bool InputBindingRegistry::isHeld(const std::string& action) const
{
    if (paused)
        return policyHeld(action, PausePolicy::AlwaysActive);

    return policyHeld(action, PausePolicy::Gameplay)
        || policyHeld(action, PausePolicy::AlwaysActive);
}

bool InputBindingRegistry::isReleased(const std::string& action) const
{
    if (paused)
        return policyReleased(action, PausePolicy::AlwaysActive);

    return policyReleased(action, PausePolicy::Gameplay)
        || policyReleased(action, PausePolicy::AlwaysActive);
}

float InputBindingRegistry::axisValue(const std::string& action) const
{
    if (paused)
        return policyAxisValue(action, PausePolicy::AlwaysActive);

    const float gameplayValue = policyAxisValue(action, PausePolicy::Gameplay);
    const float alwaysValue = policyAxisValue(action, PausePolicy::AlwaysActive);
    return std::fabs(alwaysValue) > std::fabs(gameplayValue) ? alwaysValue : gameplayValue;
}

bool InputBindingRegistry::isSimulationPressed(const std::string& action) const
{
    if (paused)
        return policySimulationPressed(action, PausePolicy::AlwaysActive);

    return policySimulationPressed(action, PausePolicy::Gameplay)
        || policySimulationPressed(action, PausePolicy::AlwaysActive);
}

bool InputBindingRegistry::isSimulationReleased(const std::string& action) const
{
    if (paused)
        return policySimulationReleased(action, PausePolicy::AlwaysActive);

    return policySimulationReleased(action, PausePolicy::Gameplay)
        || policySimulationReleased(action, PausePolicy::AlwaysActive);
}

double InputBindingRegistry::mouseX() const
{
    return currentMouseX;
}

double InputBindingRegistry::mouseY() const
{
    return currentMouseY;
}

double InputBindingRegistry::scrollX() const
{
    return currentScrollX;
}

double InputBindingRegistry::scrollY() const
{
    return currentScrollY;
}

std::optional<InputTrigger> InputBindingRegistry::getLastPressedTrigger() const
{
    return lastPressedTrigger;
}

void InputBindingRegistry::setPaused(bool pausedValue)
{
    if (paused != pausedValue)
        clearSimulationEdges();
    paused = pausedValue;
}

bool InputBindingRegistry::isPaused() const
{
    return paused;
}

void InputBindingRegistry::update(const InputSnapshot& rawInput)
{
    applyPendingContextOps();
    currentMouseX = rawInput.mouseX();
    currentMouseY = rawInput.mouseY();
    currentScrollX = rawInput.scrollX();
    currentScrollY = rawInput.scrollY();

    const ModifierFlags currentModifiers = getCurrentModifiers(rawInput);
    lastPressedTrigger.reset();
    for (int keyCode = static_cast<int>(GtsKey::Unknown) + 1;
         keyCode < static_cast<int>(GtsKey::COUNT);
         ++keyCode)
    {
        const auto key = static_cast<GtsKey>(keyCode);
        if (!rawInput.isKeyPressed(key))
            continue;

        ModifierFlags modifiers = currentModifiers;
        if (isModifierKey(key))
            modifiers = modifiers & ~modifierFlagForKey(key);

        lastPressedTrigger = InputTrigger{
            InputTrigger::Type::Key,
            keyCode,
            modifiers
        };
        break;
    }

    if (!lastPressedTrigger.has_value())
    {
        constexpr int MaxMouseButtons = 8;
        for (int button = 0; button < MaxMouseButtons; ++button)
        {
            if (!rawInput.isMouseButtonPressed(button))
                continue;

            lastPressedTrigger = InputTrigger{
                InputTrigger::Type::MouseButton,
                button,
                currentModifiers
            };
            break;
        }
    }

    for (auto& [action, state] : actionStates)
    {
        state.gameplay.previous = state.gameplay.current;
        state.gameplay.current = false;
        state.gameplay.axisPrevious = state.gameplay.axisCurrent;
        state.gameplay.axisCurrent = 0.0f;

        state.alwaysActive.previous = state.alwaysActive.current;
        state.alwaysActive.current = false;
        state.alwaysActive.axisPrevious = state.alwaysActive.axisCurrent;
        state.alwaysActive.axisCurrent = 0.0f;
    }

    std::unordered_set<ConsumedInput, ConsumedInputHash> consumed;
    consumed.reserve(bindings.size());
    std::unordered_set<std::string> queuedPressedActions;
    std::unordered_set<std::string> queuedReleasedActions;

    auto processContext = [&](const std::string& context)
    {
        for (const auto& binding : bindings)
        {
            if (binding.context != context)
                continue;
            if (!matchesTrigger(rawInput, binding.trigger, currentModifiers))
                continue;

            const ConsumedInput consumedInput = toConsumedInput(binding.trigger);
            if (consumed.contains(consumedInput))
                continue;

            ActionState& actionState = actionStates[binding.action];
            PolicyState& policyState = binding.pausePolicy == PausePolicy::AlwaysActive
                ? actionState.alwaysActive
                : actionState.gameplay;
            if (signalActive(rawInput, binding))
            {
                policyState.current = true;
                if (binding.mode == ActivationMode::Pressed
                    && queuedPressedActions.insert(binding.action).second)
                {
                    queueSimulationEdge(binding);
                }
                else if (binding.mode == ActivationMode::Released
                    && queuedReleasedActions.insert(binding.action).second)
                {
                    queueSimulationEdge(binding);
                }
            }

            const float signalAxis = signalAxisValue(rawInput, binding);
            if (std::fabs(signalAxis) > std::fabs(policyState.axisCurrent))
                policyState.axisCurrent = signalAxis;

            if (!binding.passthrough)
                consumed.insert(consumedInput);
        }
    };

    for (auto it = activeContexts.rbegin(); it != activeContexts.rend(); ++it)
        processContext(*it);

    processContext("");
}

void InputBindingRegistry::finishSimulationTick()
{
    for (auto& [_, edge] : simulationEdges)
    {
        if (edge.gameplayPressed > 0) --edge.gameplayPressed;
        if (edge.gameplayReleased > 0) --edge.gameplayReleased;
        if (edge.alwaysActivePressed > 0) --edge.alwaysActivePressed;
        if (edge.alwaysActiveReleased > 0) --edge.alwaysActiveReleased;
    }
}

void InputBindingRegistry::clearSimulationEdges()
{
    for (auto& [_, edge] : simulationEdges)
        edge = {};
}

std::vector<InputTrigger> InputBindingRegistry::getTriggersForAction(const std::string& action) const
{
    std::vector<InputTrigger> result;
    const auto it = bindingsByAction.find(action);
    if (it == bindingsByAction.end())
        return result;

    result.reserve(it->second.size());
    for (size_t index : it->second)
        result.push_back(bindings[index].trigger);
    return result;
}

std::optional<std::string> InputBindingRegistry::getActionForTrigger(const InputTrigger& trigger,
                                                                     const std::string& context) const
{
    for (const auto& binding : bindings)
    {
        if (binding.trigger != trigger)
            continue;
        if (binding.context != context)
            continue;
        return binding.action;
    }

    if (!context.empty())
    {
        for (const auto& binding : bindings)
        {
            if (binding.trigger == trigger && binding.context.empty())
                return binding.action;
        }
    }

    return std::nullopt;
}

std::vector<std::string> InputBindingRegistry::getAllActions() const
{
    std::vector<std::string> actions;
    actions.reserve(bindingsByAction.size());
    for (const auto& [action, _] : bindingsByAction)
        actions.push_back(action);
    std::sort(actions.begin(), actions.end());
    return actions;
}

std::vector<InputBinding> InputBindingRegistry::getBindingsForContext(const std::string& context) const
{
    std::vector<InputBinding> result;
    for (const auto& binding : bindings)
    {
        if (binding.context == context)
            result.push_back(binding);
    }
    return result;
}

std::optional<InputBindingRegistry::BindingConflict> InputBindingRegistry::checkConflict(
    const InputTrigger& trigger,
    const std::string& context) const
{
    for (const auto& binding : bindings)
    {
        if (binding.trigger == trigger && binding.context == context)
            return BindingConflict{binding.action, binding.context, binding.trigger};
    }

    return std::nullopt;
}

void InputBindingRegistry::rebuildActionIndex()
{
    bindingsByAction.clear();
    bindingsByAction.reserve(bindings.size());

    for (size_t i = 0; i < bindings.size(); ++i)
    {
        bindingsByAction[bindings[i].action].push_back(i);
        ensureActionState(bindings[i].action);
    }
}

void InputBindingRegistry::ensureActionState(const std::string& action)
{
    actionStates.try_emplace(action);
    simulationEdges.try_emplace(action);
}

void InputBindingRegistry::applyPendingContextOps()
{
    const bool hadPendingContextOps = !pendingContextOps.empty();
    for (const auto& op : pendingContextOps)
    {
        switch (op.type)
        {
            case PendingContextOpType::Push:
            {
                activeContexts.erase(std::remove(activeContexts.begin(), activeContexts.end(), op.context),
                                     activeContexts.end());
                activeContexts.push_back(op.context);
                break;
            }
            case PendingContextOpType::Pop:
                activeContexts.erase(std::remove(activeContexts.begin(), activeContexts.end(), op.context),
                                     activeContexts.end());
                break;
            case PendingContextOpType::Clear:
                activeContexts.clear();
                break;
        }
    }

    pendingContextOps.clear();
    if (hadPendingContextOps)
        clearActionRuntimeState();
}

void InputBindingRegistry::queueSimulationEdge(const InputBinding& binding)
{
    auto& edge = simulationEdges[binding.action];
    uint32_t* counter = nullptr;
    if (binding.pausePolicy == PausePolicy::AlwaysActive)
    {
        counter = binding.mode == ActivationMode::Pressed
            ? &edge.alwaysActivePressed
            : &edge.alwaysActiveReleased;
    }
    else
    {
        counter = binding.mode == ActivationMode::Pressed
            ? &edge.gameplayPressed
            : &edge.gameplayReleased;
    }

    if (*counter < std::numeric_limits<uint32_t>::max())
        ++(*counter);
}

void InputBindingRegistry::clearActionRuntimeState()
{
    for (auto& [_, state] : actionStates)
        state = {};
    clearSimulationEdges();
    lastPressedTrigger.reset();
}

ModifierFlags InputBindingRegistry::getCurrentModifiers(const InputSnapshot& rawInput)
{
    return rawInput.modifiers;
}

bool InputBindingRegistry::isTriggerSupported(const InputTrigger& trigger)
{
    return trigger.type == InputTrigger::Type::Key
        || trigger.type == InputTrigger::Type::MouseButton;
}

bool InputBindingRegistry::matchesTrigger(const InputSnapshot& rawInput,
                                         const InputTrigger& trigger,
                                         ModifierFlags currentModifiers)
{
    if (!isTriggerSupported(trigger))
        return false;

    ModifierFlags effectiveModifiers = currentModifiers;
    if (trigger.type == InputTrigger::Type::Key)
    {
        const auto key = static_cast<GtsKey>(trigger.code);
        if (isModifierKey(key))
            effectiveModifiers = effectiveModifiers & ~modifierFlagForKey(key);
    }

    if (effectiveModifiers != trigger.modifiers)
        return false;

    switch (trigger.type)
    {
        case InputTrigger::Type::Key:
        {
            const auto key = static_cast<GtsKey>(trigger.code);
            return rawInput.isKeyDown(key)
                || rawInput.isKeyPressed(key)
                || rawInput.isKeyReleased(key);
        }
        case InputTrigger::Type::MouseButton:
            return rawInput.isMouseButtonDown(trigger.code)
                || rawInput.isMouseButtonPressed(trigger.code)
                || rawInput.isMouseButtonReleased(trigger.code);
        case InputTrigger::Type::GamepadButton:
        case InputTrigger::Type::GamepadAxis:
            return false;
    }

    return false;
}

bool InputBindingRegistry::signalActive(const InputSnapshot& rawInput, const InputBinding& binding)
{
    if (!isTriggerSupported(binding.trigger))
        return false;

    switch (binding.mode)
    {
        case ActivationMode::Pressed:
            if (binding.trigger.type == InputTrigger::Type::MouseButton)
                return rawInput.isMouseButtonPressed(binding.trigger.code);
            return rawInput.isKeyPressed(static_cast<GtsKey>(binding.trigger.code));
        case ActivationMode::Released:
            if (binding.trigger.type == InputTrigger::Type::MouseButton)
                return rawInput.isMouseButtonReleased(binding.trigger.code);
            return rawInput.isKeyReleased(static_cast<GtsKey>(binding.trigger.code));
        case ActivationMode::Held:
            if (binding.trigger.type == InputTrigger::Type::MouseButton)
                return rawInput.isMouseButtonDown(binding.trigger.code);
            return rawInput.isKeyDown(static_cast<GtsKey>(binding.trigger.code));
        case ActivationMode::Repeated:
            return false;
    }

    return false;
}

float InputBindingRegistry::signalAxisValue(const InputSnapshot& rawInput, const InputBinding& binding)
{
    if (binding.trigger.type == InputTrigger::Type::GamepadAxis)
        return 0.0f;

    if (binding.mode != ActivationMode::Held || binding.trigger.type != InputTrigger::Type::Key)
        return 0.0f;

    const auto key = static_cast<GtsKey>(binding.trigger.code);
    return rawInput.isKeyDown(key) ? 1.0f : 0.0f;
}

InputBindingRegistry::ConsumedInput InputBindingRegistry::toConsumedInput(const InputTrigger& trigger)
{
    return ConsumedInput{
        trigger.type,
        trigger.code,
        trigger.axisIndex,
        trigger.axisDirection
    };
}

bool InputBindingRegistry::policyPressed(const std::string& action, PausePolicy policy) const
{
    const auto it = actionStates.find(action);
    if (it == actionStates.end())
        return false;

    const PolicyState& state = policy == PausePolicy::AlwaysActive
        ? it->second.alwaysActive
        : it->second.gameplay;
    return state.current && !state.previous;
}

bool InputBindingRegistry::policyHeld(const std::string& action, PausePolicy policy) const
{
    const auto it = actionStates.find(action);
    if (it == actionStates.end())
        return false;

    const PolicyState& state = policy == PausePolicy::AlwaysActive
        ? it->second.alwaysActive
        : it->second.gameplay;
    return state.current;
}

bool InputBindingRegistry::policyReleased(const std::string& action, PausePolicy policy) const
{
    const auto it = actionStates.find(action);
    if (it == actionStates.end())
        return false;

    const PolicyState& state = policy == PausePolicy::AlwaysActive
        ? it->second.alwaysActive
        : it->second.gameplay;
    return !state.current && state.previous;
}

float InputBindingRegistry::policyAxisValue(const std::string& action, PausePolicy policy) const
{
    const auto it = actionStates.find(action);
    if (it == actionStates.end())
        return 0.0f;

    const PolicyState& state = policy == PausePolicy::AlwaysActive
        ? it->second.alwaysActive
        : it->second.gameplay;
    return state.axisCurrent;
}

bool InputBindingRegistry::policySimulationPressed(const std::string& action,
                                                   PausePolicy policy) const
{
    const auto it = simulationEdges.find(action);
    if (it == simulationEdges.end())
        return false;

    const SimulationEdgeState& state = it->second;
    return policy == PausePolicy::AlwaysActive
        ? state.alwaysActivePressed > 0
        : state.gameplayPressed > 0;
}

bool InputBindingRegistry::policySimulationReleased(const std::string& action,
                                                    PausePolicy policy) const
{
    const auto it = simulationEdges.find(action);
    if (it == simulationEdges.end())
        return false;

    const SimulationEdgeState& state = it->second;
    return policy == PausePolicy::AlwaysActive
        ? state.alwaysActiveReleased > 0
        : state.gameplayReleased > 0;
}
