#pragma once

#include <string>
#include <vector>

#include "DialogueTypes.h"

namespace gts::dialogue
{
    struct DialogueStartedEvent
    {
        std::string graphId;
        std::string nodeId;
    };

    struct DialogueNodeChangedEvent
    {
        std::string graphId;
        std::string nodeId;
        std::string speaker;
        std::string text;
    };

    struct DialogueChoicesChangedEvent
    {
        std::string graphId;
        std::string nodeId;
        std::vector<DialogueVisibleChoice> choices;
    };

    struct DialogueActionRequestedEvent
    {
        std::string graphId;
        std::string nodeId;
        DialogueAction action;
    };

    struct DialogueEndedEvent
    {
        std::string graphId;
    };
} // namespace gts::dialogue
