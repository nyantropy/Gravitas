#pragma once

#include <functional>
#include <string>
#include <unordered_map>
#include <utility>

#include "DialogueTypes.h"

class ECSWorld;
struct EcsControllerContext;

namespace gts::dialogue
{
    struct DialogueContext
    {
        ECSWorld* world = nullptr;
        const EcsControllerContext* frame = nullptr;
        std::string graphId;
        std::string nodeId;
        std::unordered_map<std::string, std::string> variables;
        std::function<void(const DialogueAction&, const DialogueContext&)> actionRequested;
    };

    using DialogueConditionHandler =
        std::function<bool(const DialogueCondition&, const DialogueContext&)>;

    using DialogueActionHandler =
        std::function<void(const DialogueAction&, DialogueContext&)>;

    class DialogueConditionRegistry
    {
    public:
        void registerCondition(const std::string& id, DialogueConditionHandler handler)
        {
            if (id.empty())
                return;

            handlers[id] = std::move(handler);
        }

        void unregisterCondition(const std::string& id)
        {
            handlers.erase(id);
        }

        bool hasCondition(const std::string& id) const
        {
            return handlers.find(id) != handlers.end();
        }

        bool evaluate(const DialogueCondition& condition, const DialogueContext& context) const
        {
            const auto it = handlers.find(condition.id);
            if (it == handlers.end())
                return false;

            return it->second(condition, context);
        }

    private:
        std::unordered_map<std::string, DialogueConditionHandler> handlers;
    };

    class DialogueActionRegistry
    {
    public:
        void registerAction(const std::string& id, DialogueActionHandler handler)
        {
            if (id.empty())
                return;

            handlers[id] = std::move(handler);
        }

        void unregisterAction(const std::string& id)
        {
            handlers.erase(id);
        }

        bool hasAction(const std::string& id) const
        {
            return handlers.find(id) != handlers.end();
        }

        bool execute(const DialogueAction& action, DialogueContext& context) const
        {
            const auto it = handlers.find(action.id);
            if (it == handlers.end())
                return false;

            it->second(action, context);
            return true;
        }

    private:
        std::unordered_map<std::string, DialogueActionHandler> handlers;
    };
} // namespace gts::dialogue
