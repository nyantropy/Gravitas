#pragma once

#include <string>
#include <unordered_map>

#include "DialogueRuntime.h"

namespace gts::dialogue
{
    struct DialogueRuntimeComponent
    {
        DialogueRuntime runtime;
    };

    struct DialogueGraphRegistryComponent
    {
        std::unordered_map<std::string, DialogueGraph> graphs;

        void addGraph(const DialogueGraph& graph)
        {
            if (!graph.id.empty())
                graphs[graph.id] = graph;
        }

        const DialogueGraph* findGraph(const std::string& graphId) const
        {
            const auto it = graphs.find(graphId);
            return it == graphs.end() ? nullptr : &it->second;
        }
    };

    struct DialogueStartRequestComponent
    {
        bool requested = false;
        std::string graphId;
        DialogueGraph graph;
        bool useInlineGraph = false;

        void requestGraphId(const std::string& inGraphId)
        {
            requested = true;
            graphId = inGraphId;
            graph = DialogueGraph{};
            useInlineGraph = false;
        }

        void requestGraph(const DialogueGraph& inGraph)
        {
            requested = true;
            graphId = inGraph.id;
            graph = inGraph;
            useInlineGraph = true;
        }
    };

    struct NPCInteractionComponent
    {
        std::string characterId;
        std::string dialogueGraphId;
    };
} // namespace gts::dialogue
