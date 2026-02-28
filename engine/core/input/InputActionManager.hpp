#pragma once

#include <array>
#include <vector>
#include <algorithm>

#include "InputManager.hpp"
#include "GtsKey.h"

// Generic action-to-key mapping layer.
//
// ActionEnum must be an enum (class) whose last enumerator is ACTION_COUNT,
// which determines the size of the internal state arrays.
//
// Usage:
//   1. Call bind() to associate one or more keys with each action.
//   2. Call update() once per frame, after InputManager::beginFrame() and
//      window event polling.
//   3. Query isActionActive / isActionPressed / isActionReleased from any
//      ECS system that has access to this manager.
//
// Multiple keys can drive the same action (any one held → action is active).
// Bindings can be changed at runtime. No heap allocations occur during
// update() or any query call.
//
// Each application can instantiate its own InputActionManager<AppAction>
// parameterized on its own action enum, keeping action definitions local
// to the project that owns them.

template<typename ActionEnum>
class InputActionManager
{
    static constexpr size_t COUNT = static_cast<size_t>(ActionEnum::ACTION_COUNT);

    std::array<std::vector<GtsKey>, COUNT> bindings{};
    std::array<bool, COUNT> currentState{};
    std::array<bool, COUNT> previousState{};

    size_t idx(ActionEnum a) const { return static_cast<size_t>(a); }

public:

    // ── binding API ───────────────────────────────────────────────────

    // Adds key as a trigger for action (duplicate-safe).
    void bind(ActionEnum action, GtsKey key)
    {
        auto& keys = bindings[idx(action)];
        for (GtsKey k : keys)
            if (k == key) return;
        keys.push_back(key);
    }

    // Removes one specific key from an action's binding list.
    void unbind(ActionEnum action, GtsKey key)
    {
        auto& keys = bindings[idx(action)];
        keys.erase(std::remove(keys.begin(), keys.end(), key), keys.end());
    }

    // Removes all key bindings for an action.
    void clearBindings(ActionEnum action)
    {
        bindings[idx(action)].clear();
    }

    // Returns the current binding list for an action (read-only).
    const std::vector<GtsKey>& getBindings(ActionEnum action) const
    {
        return bindings[idx(action)];
    }

    // ── per-frame update ──────────────────────────────────────────────

    // Call once per frame after InputManager::beginFrame() + event poll.
    void update(const InputManager& input)
    {
        previousState = currentState;

        for (size_t i = 0; i < COUNT; ++i)
        {
            bool active = false;
            for (GtsKey key : bindings[i])
                if (input.isKeyDown(key)) { active = true; break; }
            currentState[i] = active;
        }
    }

    // ── query API ─────────────────────────────────────────────────────

    // True every frame the action's keys are held.
    bool isActionActive(ActionEnum action) const
    {
        return currentState[idx(action)];
    }

    // True only on the frame the action first became active.
    bool isActionPressed(ActionEnum action) const
    {
        size_t i = idx(action);
        return currentState[i] && !previousState[i];
    }

    // True only on the frame the action was released.
    bool isActionReleased(ActionEnum action) const
    {
        size_t i = idx(action);
        return !currentState[i] && previousState[i];
    }
};
