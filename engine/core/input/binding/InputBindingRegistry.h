#pragma once

#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "InputBinding.h"
#include "InputSnapshot.hpp"

class InputBindingRegistry
{
public:
    void bind(const InputBinding& binding);
    void bind(const std::string& action, InputTrigger trigger,
              ActivationMode mode = ActivationMode::Pressed,
              const std::string& context = "",
              PausePolicy pausePolicy = PausePolicy::Gameplay);

    void unbind(const std::string& action, const InputTrigger& trigger);
    void unbindAll(const std::string& action);
    void rebind(const std::string& action,
                const InputTrigger& oldTrigger,
                const InputTrigger& newTrigger);

    void loadBindings(const std::vector<InputBinding>& bindings);
    std::vector<InputBinding> exportBindings() const;
    void clearAllBindings();

    void pushContext(const std::string& context);
    void popContext(const std::string& context);
    void clearContextStack();
    bool isContextActive(const std::string& context) const;

    bool isPressed(const std::string& action) const;
    bool isHeld(const std::string& action) const;
    bool isReleased(const std::string& action) const;
    float axisValue(const std::string& action) const;

    void setPaused(bool paused);
    bool isPaused() const;

    void update(const InputSnapshot& rawInput);

    std::vector<InputTrigger> getTriggersForAction(const std::string& action) const;
    std::optional<std::string> getActionForTrigger(const InputTrigger& trigger,
                                                   const std::string& context = "") const;
    std::vector<std::string> getAllActions() const;
    std::vector<InputBinding> getBindingsForContext(const std::string& context) const;

    struct BindingConflict
    {
        std::string existingAction;
        std::string existingContext;
        InputTrigger trigger;
    };

    std::optional<BindingConflict> checkConflict(const InputTrigger& trigger,
                                                 const std::string& context) const;

private:
    struct PolicyState
    {
        bool  current = false;
        bool  previous = false;
        float axisCurrent = 0.0f;
        float axisPrevious = 0.0f;
    };

    struct ActionState
    {
        PolicyState gameplay;
        PolicyState alwaysActive;
    };

    enum class PendingContextOpType
    {
        Push,
        Pop,
        Clear
    };

    struct PendingContextOp
    {
        PendingContextOpType type = PendingContextOpType::Push;
        std::string          context;
    };

    struct InputTriggerHash
    {
        size_t operator()(const InputTrigger& trigger) const
        {
            return trigger.hash();
        }
    };

    struct ConsumedInput
    {
        InputTrigger::Type type = InputTrigger::Type::Key;
        int                code = 0;
        int                axisIndex = -1;
        float              axisDirection = 0.0f;

        bool operator==(const ConsumedInput& other) const;
        size_t hash() const;
    };

    struct ConsumedInputHash
    {
        size_t operator()(const ConsumedInput& input) const
        {
            return input.hash();
        }
    };

    std::vector<InputBinding> bindings;
    std::vector<std::string> activeContexts;
    std::vector<PendingContextOp> pendingContextOps;
    std::unordered_map<std::string, ActionState> actionStates;
    std::unordered_map<std::string, std::vector<size_t>> bindingsByAction;
    bool paused = false;

    void rebuildActionIndex();
    void ensureActionState(const std::string& action);
    void applyPendingContextOps();

    static ModifierFlags getCurrentModifiers(const InputSnapshot& rawInput);
    static bool isTriggerSupported(const InputTrigger& trigger);
    static bool matchesTrigger(const InputSnapshot& rawInput,
                               const InputTrigger& trigger,
                               ModifierFlags currentModifiers);
    static bool signalActive(const InputSnapshot& rawInput, const InputBinding& binding);
    static float signalAxisValue(const InputSnapshot& rawInput, const InputBinding& binding);
    static ConsumedInput toConsumedInput(const InputTrigger& trigger);

    bool policyPressed(const std::string& action, PausePolicy policy) const;
    bool policyHeld(const std::string& action, PausePolicy policy) const;
    bool policyReleased(const std::string& action, PausePolicy policy) const;
    float policyAxisValue(const std::string& action, PausePolicy policy) const;
};
