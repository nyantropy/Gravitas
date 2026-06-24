#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "DialogueRegistry.h"
#include "DialogueTypes.h"

namespace gts::dialogue
{
    enum class DialogueRuntimeStatus
    {
        Idle = 0,
        Active,
        Ended
    };

    class DialogueRuntime
    {
    public:
        DialogueRuntime() = default;

        explicit DialogueRuntime(DialogueRuntimeConfig inConfig)
            : config(inConfig)
        {
        }

        void setConfig(const DialogueRuntimeConfig& inConfig)
        {
            config = inConfig;
        }

        const DialogueRuntimeConfig& getConfig() const
        {
            return config;
        }

        DialogueConditionRegistry& conditions()
        {
            return conditionRegistry;
        }

        const DialogueConditionRegistry& conditions() const
        {
            return conditionRegistry;
        }

        DialogueActionRegistry& actions()
        {
            return actionRegistry;
        }

        const DialogueActionRegistry& actions() const
        {
            return actionRegistry;
        }

        bool start(const DialogueGraph& graph, DialogueContext context = {})
        {
            activeGraph = graph;
            status = DialogueRuntimeStatus::Active;
            currentNodeId.clear();
            visibleChoices.clear();
            revision++;

            context.graphId = activeGraph.id;
            return enterNode(activeGraph.startNode, context);
        }

        bool start(DialogueGraph&& graph, DialogueContext context = {})
        {
            activeGraph = std::move(graph);
            status = DialogueRuntimeStatus::Active;
            currentNodeId.clear();
            visibleChoices.clear();
            revision++;

            context.graphId = activeGraph.id;
            return enterNode(activeGraph.startNode, context);
        }

        void end(DialogueContext context = {})
        {
            if (!isActive())
                return;

            if (ending)
            {
                currentNodeId.clear();
                visibleChoices.clear();
                status = DialogueRuntimeStatus::Ended;
                revision++;
                return;
            }

            ending = true;
            context.graphId = activeGraph.id;
            context.nodeId = currentNodeId;
            executeCurrentExitActions(context);
            ending = false;
            currentNodeId.clear();
            visibleChoices.clear();
            status = DialogueRuntimeStatus::Ended;
            revision++;
        }

        bool advance(DialogueContext context = {})
        {
            if (!isActive())
                return false;

            refreshVisibleChoices(context);
            if (!visibleChoices.empty())
                return false;

            const DialogueNode* node = getCurrentNode();
            if (node != nullptr && !node->nextNode.empty())
            {
                const std::string targetNode = node->nextNode;
                context.graphId = activeGraph.id;
                context.nodeId = currentNodeId;
                executeCurrentExitActions(context);
                return enterNode(targetNode, context);
            }

            end(context);
            return true;
        }

        bool selectChoice(size_t visibleChoiceIndex, DialogueContext context = {})
        {
            if (!isActive())
                return false;

            refreshVisibleChoices(context);
            if (visibleChoiceIndex >= visibleChoices.size())
                return false;

            const DialogueVisibleChoice visibleChoice = visibleChoices[visibleChoiceIndex];
            const DialogueNode* node = getCurrentNode();
            if (node == nullptr || visibleChoice.sourceIndex >= node->choices.size())
            {
                if (config.endOnMissingNode)
                    end(context);
                return false;
            }

            DialogueChoice choice = node->choices[visibleChoice.sourceIndex];
            context.graphId = activeGraph.id;
            context.nodeId = currentNodeId;

            if (!executeActions(choice.actions, context))
                return false;

            executeCurrentExitActions(context);
            if (choice.targetNode.empty())
            {
                end(context);
                return true;
            }

            return enterNode(choice.targetNode, context);
        }

        void refreshVisibleChoices(DialogueContext context = {})
        {
            visibleChoices.clear();

            const DialogueNode* node = getCurrentNode();
            if (node == nullptr)
                return;

            context.graphId = activeGraph.id;
            context.nodeId = currentNodeId;

            for (size_t i = 0; i < node->choices.size(); ++i)
            {
                const DialogueChoice& choice = node->choices[i];
                if (!conditionsPass(choice.conditions, context))
                    continue;

                visibleChoices.push_back(DialogueVisibleChoice{
                    i,
                    choice.text,
                    choice.targetNode
                });
            }
        }

        bool isActive() const
        {
            return status == DialogueRuntimeStatus::Active;
        }

        DialogueRuntimeStatus getStatus() const
        {
            return status;
        }

        const DialogueGraph& getGraph() const
        {
            return activeGraph;
        }

        const DialogueNode* getCurrentNode() const
        {
            if (!isActive() || currentNodeId.empty())
                return nullptr;

            return activeGraph.findNode(currentNodeId);
        }

        const std::string& getCurrentNodeId() const
        {
            return currentNodeId;
        }

        const std::vector<DialogueVisibleChoice>& getVisibleChoices() const
        {
            return visibleChoices;
        }

        uint64_t getRevision() const
        {
            return revision;
        }

    private:
        DialogueRuntimeConfig config;
        DialogueConditionRegistry conditionRegistry;
        DialogueActionRegistry actionRegistry;
        DialogueGraph activeGraph;
        DialogueRuntimeStatus status = DialogueRuntimeStatus::Idle;
        std::string currentNodeId;
        std::vector<DialogueVisibleChoice> visibleChoices;
        uint64_t revision = 1;
        bool ending = false;

        bool enterNode(const std::string& nodeId, DialogueContext context)
        {
            const DialogueNode* node = activeGraph.findNode(nodeId);
            if (node == nullptr)
            {
                if (config.endOnMissingNode)
                    end(context);
                return false;
            }

            currentNodeId = nodeId;
            context.graphId = activeGraph.id;
            context.nodeId = currentNodeId;

            if (!executeActions(node->onEnterActions, context))
                return false;

            if (node->end)
            {
                end(context);
                return true;
            }

            refreshVisibleChoices(context);
            revision++;
            return true;
        }

        void executeCurrentExitActions(DialogueContext& context)
        {
            const DialogueNode* node = getCurrentNode();
            if (node == nullptr)
                return;

            context.graphId = activeGraph.id;
            context.nodeId = currentNodeId;
            executeActions(node->onExitActions, context);
        }

        bool executeActions(const std::vector<DialogueAction>& actionsToRun,
                            DialogueContext& context)
        {
            for (const DialogueAction& action : actionsToRun)
            {
                if (context.actionRequested)
                    context.actionRequested(action, context);

                const bool executed = actionRegistry.execute(action, context);
                if (executed)
                    continue;

                if (config.unknownActionPolicy == DialogueUnknownActionPolicy::EndDialogue)
                {
                    end(context);
                    return false;
                }
            }

            return true;
        }

        bool conditionsPass(const std::vector<DialogueCondition>& conditionsToCheck,
                            const DialogueContext& context) const
        {
            for (const DialogueCondition& condition : conditionsToCheck)
            {
                if (!conditionRegistry.evaluate(condition, context))
                    return false;
            }

            return true;
        }
    };
} // namespace gts::dialogue
