#pragma once

#include <string>
#include <unordered_map>
#include <vector>

namespace gts::dialogue
{
    struct DialogueAction
    {
        std::string id;
        std::unordered_map<std::string, std::string> args;
    };

    struct DialogueCondition
    {
        std::string id;
        std::unordered_map<std::string, std::string> args;
    };

    struct DialogueChoice
    {
        std::string text;
        std::string targetNode;
        std::vector<DialogueCondition> conditions;
        std::vector<DialogueAction> actions;
    };

    struct DialogueNode
    {
        std::string id;
        std::string speaker;
        std::string text;
        std::string nextNode;
        std::vector<DialogueChoice> choices;
        std::vector<DialogueAction> onEnterActions;
        std::vector<DialogueAction> onExitActions;
        bool end = false;
    };

    struct DialogueGraph
    {
        std::string id;
        std::string startNode;
        std::unordered_map<std::string, DialogueNode> nodes;

        const DialogueNode* findNode(const std::string& nodeId) const
        {
            const auto it = nodes.find(nodeId);
            return it == nodes.end() ? nullptr : &it->second;
        }
    };

    struct DialogueVisibleChoice
    {
        size_t sourceIndex = 0;
        std::string text;
        std::string targetNode;
    };

    enum class DialogueUnknownActionPolicy
    {
        Ignore = 0,
        EndDialogue
    };

    struct DialogueRuntimeConfig
    {
        DialogueUnknownActionPolicy unknownActionPolicy = DialogueUnknownActionPolicy::Ignore;
        bool endOnMissingNode = true;
    };
} // namespace gts::dialogue
