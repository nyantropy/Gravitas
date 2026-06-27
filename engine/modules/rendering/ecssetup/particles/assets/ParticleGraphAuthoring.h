#pragma once

#include <algorithm>
#include <string>
#include <vector>

#include "ParticleEffectAsset.h"

namespace gts::particles
{
    inline std::string particleGraphUniqueId(const std::vector<std::string>& used, const std::string& base)
    {
        if (std::find(used.begin(), used.end(), base) == used.end())
            return base;

        for (uint32_t suffix = 2; suffix < 10000u; ++suffix)
        {
            const std::string candidate = base + "_" + std::to_string(suffix);
            if (std::find(used.begin(), used.end(), candidate) == used.end())
                return candidate;
        }
        return base + "_9999";
    }

    inline ParticleModuleInstance* findParticleGraphModule(ParticleEffectEmitter& emitter,
                                                           const std::string&     stableId)
    {
        const auto it = std::find_if(emitter.modules.begin(),
                                     emitter.modules.end(),
                                     [&](const ParticleModuleInstance& module)
                                     {
                                         return module.stableId == stableId;
                                     });
        return it == emitter.modules.end() ? nullptr : &*it;
    }

    inline const ParticleModuleInstance* findParticleGraphModule(const ParticleEffectEmitter& emitter,
                                                                 const std::string&           stableId)
    {
        const auto it = std::find_if(emitter.modules.begin(),
                                     emitter.modules.end(),
                                     [&](const ParticleModuleInstance& module)
                                     {
                                         return module.stableId == stableId;
                                     });
        return it == emitter.modules.end() ? nullptr : &*it;
    }

    inline ParticleGraphNode* findParticleGraphNode(ParticleEffectGraph& graph, const std::string& nodeId)
    {
        const auto it = std::find_if(graph.nodes.begin(),
                                     graph.nodes.end(),
                                     [&](const ParticleGraphNode& node)
                                     {
                                         return node.id == nodeId;
                                     });
        return it == graph.nodes.end() ? nullptr : &*it;
    }

    inline const ParticleGraphNode* findParticleGraphNode(const ParticleEffectGraph& graph, const std::string& nodeId)
    {
        const auto it = std::find_if(graph.nodes.begin(),
                                     graph.nodes.end(),
                                     [&](const ParticleGraphNode& node)
                                     {
                                         return node.id == nodeId;
                                     });
        return it == graph.nodes.end() ? nullptr : &*it;
    }

    inline ParticleGraphNode* findParticleGraphNodeForModule(ParticleEffectGraph& graph,
                                                             const std::string&   moduleStableId)
    {
        const auto it = std::find_if(graph.nodes.begin(),
                                     graph.nodes.end(),
                                     [&](const ParticleGraphNode& node)
                                     {
                                         return node.moduleStableId == moduleStableId;
                                     });
        return it == graph.nodes.end() ? nullptr : &*it;
    }

    inline const ParticleGraphNode* findParticleGraphNodeForModule(const ParticleEffectGraph& graph,
                                                                   const std::string&         moduleStableId)
    {
        const auto it = std::find_if(graph.nodes.begin(),
                                     graph.nodes.end(),
                                     [&](const ParticleGraphNode& node)
                                     {
                                         return node.moduleStableId == moduleStableId;
                                     });
        return it == graph.nodes.end() ? nullptr : &*it;
    }

    inline const ParticleGraphNode* findParticleGraphNodeForType(const ParticleEffectGraph& graph,
                                                                 const std::string&         typeId)
    {
        const auto it = std::find_if(graph.nodes.begin(),
                                     graph.nodes.end(),
                                     [&](const ParticleGraphNode& node)
                                     {
                                         return node.typeId == typeId;
                                     });
        return it == graph.nodes.end() ? nullptr : &*it;
    }

    inline ParticleGraphLink* findParticleGraphLink(ParticleEffectGraph& graph,
                                                    const std::string&   fromNodeId,
                                                    const std::string&   fromPortId,
                                                    const std::string&   toNodeId,
                                                    const std::string&   toPortId)
    {
        const auto it = std::find_if(graph.links.begin(),
                                     graph.links.end(),
                                     [&](const ParticleGraphLink& link)
                                     {
                                         return link.fromNodeId == fromNodeId && link.fromPortId == fromPortId &&
                                                link.toNodeId == toNodeId && link.toPortId == toPortId;
                                     });
        return it == graph.links.end() ? nullptr : &*it;
    }

    inline const ParticleGraphLink* findParticleGraphLink(const ParticleEffectGraph& graph,
                                                          const std::string&         fromNodeId,
                                                          const std::string&         fromPortId,
                                                          const std::string&         toNodeId,
                                                          const std::string&         toPortId)
    {
        const auto it = std::find_if(graph.links.begin(),
                                     graph.links.end(),
                                     [&](const ParticleGraphLink& link)
                                     {
                                         return link.fromNodeId == fromNodeId && link.fromPortId == fromPortId &&
                                                link.toNodeId == toNodeId && link.toPortId == toPortId;
                                     });
        return it == graph.links.end() ? nullptr : &*it;
    }

    inline std::vector<std::string> particleGraphNodeIds(const ParticleEffectGraph& graph)
    {
        std::vector<std::string> ids;
        ids.reserve(graph.nodes.size());
        for (const ParticleGraphNode& node : graph.nodes)
            ids.push_back(node.id);
        return ids;
    }

    inline std::vector<std::string> particleGraphLinkIds(const ParticleEffectGraph& graph)
    {
        std::vector<std::string> ids;
        ids.reserve(graph.links.size());
        for (const ParticleGraphLink& link : graph.links)
            ids.push_back(link.id);
        return ids;
    }

    inline std::vector<std::string> particleGraphFrameIds(const ParticleEffectGraph& graph)
    {
        std::vector<std::string> ids;
        ids.reserve(graph.frames.size());
        for (const ParticleGraphFrame& frame : graph.frames)
            ids.push_back(frame.id);
        return ids;
    }

    inline ParticleGraphNode makeParticleGraphNode(const ParticleModuleInstance& module, size_t index)
    {
        ParticleGraphNode node;
        const ParticleModuleDefinition* definition = findParticleModuleDefinition(module.typeId);
        const uint32_t stage = definition == nullptr ? executionStageOrder(ParticleModuleExecutionStage::Update)
                                                     : executionStageOrder(definition->executionStage);
        node.id             = module.stableId.empty() ? "node_" + std::to_string(index + 1) : module.stableId;
        node.moduleStableId = module.stableId;
        node.typeId         = module.typeId;
        node.displayName    = module.displayName;
        node.position       = {0.12f + static_cast<float>(stage) * 0.25f, 0.12f + static_cast<float>(index) * 0.10f};
        return node;
    }

    inline void pruneParticleGraphLinks(ParticleEffectGraph& graph)
    {
        graph.links.erase(std::remove_if(graph.links.begin(),
                                         graph.links.end(),
                                         [&](const ParticleGraphLink& link)
                                         {
                                             return findParticleGraphNode(graph, link.fromNodeId) == nullptr ||
                                                    findParticleGraphNode(graph, link.toNodeId) == nullptr;
                                         }),
                          graph.links.end());
    }

    inline bool connectParticleGraphLink(ParticleEffectGraph& graph,
                                         const std::string&   fromNodeId,
                                         const std::string&   fromPortId,
                                         const std::string&   toNodeId,
                                         const std::string&   toPortId)
    {
        if (fromNodeId.empty() || fromPortId.empty() || toNodeId.empty() || toPortId.empty())
            return false;
        if (findParticleGraphLink(graph, fromNodeId, fromPortId, toNodeId, toPortId) != nullptr)
            return false;

        ParticleGraphLink link;
        link.id         = particleGraphUniqueId(particleGraphLinkIds(graph), fromNodeId + "_to_" + toNodeId);
        link.fromNodeId = fromNodeId;
        link.fromPortId = fromPortId;
        link.toNodeId   = toNodeId;
        link.toPortId   = toPortId;
        graph.links.push_back(std::move(link));
        return true;
    }

    inline void connectParticleGraphDependencies(ParticleEffectGraph& graph)
    {
        for (const ParticleGraphNode& node : graph.nodes)
        {
            const ParticleModuleDefinition* definition = findParticleModuleDefinition(node.typeId);
            if (definition == nullptr)
                continue;

            for (const ParticleModuleDependency& dependencyInfo : definition->dependencies)
            {
                if (dependencyInfo.outputId.empty())
                    continue;
                const ParticleGraphNode* source = findParticleGraphNodeForType(graph, dependencyInfo.typeId);
                if (source == nullptr)
                    continue;
                connectParticleGraphLink(
                    graph, source->id, dependencyInfo.outputId, node.id, dependencyInfo.outputId);
            }
        }
    }

    inline void syncParticleGraphWithModules(ParticleEffectEmitter& emitter)
    {
        ParticleEffectGraph& graph = emitter.graph;
        const bool           graphWasEmpty = graph.nodes.empty();
        graph.schemaVersion                = CurrentParticleGraphSchemaVersion;

        graph.nodes.erase(std::remove_if(graph.nodes.begin(),
                                         graph.nodes.end(),
                                         [&](const ParticleGraphNode& node)
                                         {
                                             return findParticleGraphModule(emitter, node.moduleStableId) == nullptr;
                                         }),
                          graph.nodes.end());

        std::vector<std::string> usedNodeIds = particleGraphNodeIds(graph);
        for (size_t i = 0; i < emitter.modules.size(); ++i)
        {
            ParticleModuleInstance& module = emitter.modules[i];
            completeModuleParameters(module);
            if (module.stableId.empty())
                module.stableId = "module_" + std::to_string(i + 1);

            ParticleGraphNode* node = findParticleGraphNodeForModule(graph, module.stableId);
            if (node == nullptr)
            {
                ParticleGraphNode created = makeParticleGraphNode(module, i);
                created.id                = particleGraphUniqueId(usedNodeIds, created.id);
                usedNodeIds.push_back(created.id);
                graph.nodes.push_back(std::move(created));
                continue;
            }

            node->typeId      = module.typeId;
            node->displayName = module.displayName;
        }

        pruneParticleGraphLinks(graph);
        if (graphWasEmpty)
            connectParticleGraphDependencies(graph);
    }

    inline bool addParticleGraphNode(ParticleEffectEmitter& emitter, const ParticleModuleDefinition& definition)
    {
        if (findParticleGraphNodeForType(emitter.graph, definition.typeId) != nullptr)
            return false;

        ParticleModuleInstance module = makeDefaultModuleInstance(definition);
        module.stableId = particleGraphUniqueId(particleGraphNodeIds(emitter.graph), definition.category);
        emitter.modules.push_back(module);
        syncParticleGraphWithModules(emitter);
        connectParticleGraphDependencies(emitter.graph);
        return true;
    }

    inline bool removeParticleGraphNode(ParticleEffectEmitter& emitter, const std::string& nodeId)
    {
        ParticleGraphNode* node = findParticleGraphNode(emitter.graph, nodeId);
        if (node == nullptr)
            return false;
        const std::string moduleStableId = node->moduleStableId;

        emitter.modules.erase(std::remove_if(emitter.modules.begin(),
                                             emitter.modules.end(),
                                             [&](const ParticleModuleInstance& module)
                                             {
                                                 return module.stableId == moduleStableId;
                                             }),
                              emitter.modules.end());
        emitter.graph.nodes.erase(std::remove_if(emitter.graph.nodes.begin(),
                                                 emitter.graph.nodes.end(),
                                                 [&](const ParticleGraphNode& candidate)
                                                 {
                                                     return candidate.id == nodeId;
                                                 }),
                                  emitter.graph.nodes.end());
        pruneParticleGraphLinks(emitter.graph);
        return true;
    }

    inline bool toggleParticleGraphDependencyLink(ParticleEffectEmitter& emitter, const std::string& nodeId)
    {
        ParticleGraphNode* node = findParticleGraphNode(emitter.graph, nodeId);
        if (node == nullptr)
            return false;

        const ParticleModuleDefinition* definition = findParticleModuleDefinition(node->typeId);
        if (definition == nullptr)
            return false;

        for (const ParticleModuleDependency& dependencyInfo : definition->dependencies)
        {
            if (dependencyInfo.outputId.empty())
                continue;
            const ParticleGraphNode* source = findParticleGraphNodeForType(emitter.graph, dependencyInfo.typeId);
            if (source == nullptr)
                continue;

            ParticleGraphLink* existing =
                findParticleGraphLink(emitter.graph, source->id, dependencyInfo.outputId, node->id, dependencyInfo.outputId);
            if (existing != nullptr)
            {
                const std::string linkId = existing->id;
                emitter.graph.links.erase(std::remove_if(emitter.graph.links.begin(),
                                                         emitter.graph.links.end(),
                                                         [&](const ParticleGraphLink& link)
                                                         {
                                                             return link.id == linkId;
                                                         }),
                                          emitter.graph.links.end());
                return true;
            }
            return connectParticleGraphLink(
                emitter.graph, source->id, dependencyInfo.outputId, node->id, dependencyInfo.outputId);
        }

        return false;
    }

    inline bool addParticleGraphFrameForNode(ParticleEffectEmitter& emitter, const std::string& nodeId)
    {
        ParticleGraphNode* node = findParticleGraphNode(emitter.graph, nodeId);
        if (node == nullptr)
            return false;

        ParticleGraphFrame frame;
        frame.id       = particleGraphUniqueId(particleGraphFrameIds(emitter.graph), "frame");
        frame.title    = "Group " + std::to_string(emitter.graph.frames.size() + 1);
        frame.position = node->position + glm::vec2{-0.04f, -0.04f};
        frame.size     = {0.24f, 0.14f};
        node->frameId  = frame.id;
        emitter.graph.frames.push_back(std::move(frame));
        return true;
    }

    inline bool addParticleGraphCommentForNode(ParticleEffectEmitter& emitter, const std::string& nodeId)
    {
        const ParticleGraphNode* node = findParticleGraphNode(emitter.graph, nodeId);
        if (node == nullptr)
            return false;

        std::vector<std::string> used;
        used.reserve(emitter.graph.comments.size());
        for (const ParticleGraphComment& comment : emitter.graph.comments)
            used.push_back(comment.id);

        ParticleGraphComment comment;
        comment.id       = particleGraphUniqueId(used, "comment");
        comment.text     = "Comment " + std::to_string(emitter.graph.comments.size() + 1);
        comment.position = node->position + glm::vec2{0.02f, -0.06f};
        emitter.graph.comments.push_back(std::move(comment));
        return true;
    }

    inline bool validateParticleEffectGraph(const ParticleEffectEmitter&                 emitter,
                                            std::vector<ParticleModuleGraphDiagnostic>* diagnostics = nullptr)
    {
        std::vector<ParticleModuleGraphDiagnostic> localDiagnostics;
        if (diagnostics == nullptr)
            diagnostics = &localDiagnostics;

        validateParticleModuleStack(emitter.modules, diagnostics);

        std::vector<std::string> nodeIds;
        std::vector<std::string> moduleStableIds;
        std::vector<std::string> frameIds;
        for (const ParticleGraphFrame& frame : emitter.graph.frames)
        {
            if (frame.id.empty())
                addGraphDiagnostic(
                    diagnostics, ParticleModuleGraphDiagnosticSeverity::Error, "graph frame has empty id");
            else if (hasDuplicateString(frameIds, frame.id))
                addGraphDiagnostic(
                    diagnostics, ParticleModuleGraphDiagnosticSeverity::Error, "graph frame id is duplicated: " + frame.id);
            frameIds.push_back(frame.id);
        }

        for (const ParticleGraphNode& node : emitter.graph.nodes)
        {
            if (node.id.empty())
                addGraphDiagnostic(
                    diagnostics, ParticleModuleGraphDiagnosticSeverity::Error, "graph node has empty id", node.typeId);
            else if (hasDuplicateString(nodeIds, node.id))
                addGraphDiagnostic(diagnostics,
                                   ParticleModuleGraphDiagnosticSeverity::Error,
                                   "graph node id is duplicated: " + node.id,
                                   node.typeId,
                                   node.moduleStableId);
            nodeIds.push_back(node.id);

            if (node.moduleStableId.empty())
                addGraphDiagnostic(diagnostics,
                                   ParticleModuleGraphDiagnosticSeverity::Error,
                                   "graph node has empty backing module id",
                                   node.typeId);
            else if (hasDuplicateString(moduleStableIds, node.moduleStableId))
                addGraphDiagnostic(diagnostics,
                                   ParticleModuleGraphDiagnosticSeverity::Error,
                                   "graph node backing module is duplicated: " + node.moduleStableId,
                                   node.typeId,
                                   node.moduleStableId);
            moduleStableIds.push_back(node.moduleStableId);

            const ParticleModuleInstance* module = findParticleGraphModule(emitter, node.moduleStableId);
            if (module == nullptr)
            {
                addGraphDiagnostic(diagnostics,
                                   ParticleModuleGraphDiagnosticSeverity::Error,
                                   "graph node has no backing module",
                                   node.typeId,
                                   node.moduleStableId);
            }
            else if (module->typeId != node.typeId)
            {
                addGraphDiagnostic(diagnostics,
                                   ParticleModuleGraphDiagnosticSeverity::Error,
                                   "graph node type does not match backing module",
                                   node.typeId,
                                   node.moduleStableId);
            }

            if (!node.frameId.empty() && std::find(frameIds.begin(), frameIds.end(), node.frameId) == frameIds.end())
            {
                addGraphDiagnostic(diagnostics,
                                   ParticleModuleGraphDiagnosticSeverity::Warning,
                                   "graph node references a missing frame: " + node.frameId,
                                   node.typeId,
                                   node.moduleStableId);
            }
        }

        for (const ParticleModuleInstance& module : emitter.modules)
        {
            if (findParticleGraphNodeForModule(emitter.graph, module.stableId) == nullptr)
            {
                addGraphDiagnostic(diagnostics,
                                   ParticleModuleGraphDiagnosticSeverity::Error,
                                   "module has no graph node",
                                   module.typeId,
                                   module.stableId);
            }
        }

        std::vector<std::string> linkIds;
        for (const ParticleGraphLink& link : emitter.graph.links)
        {
            if (link.id.empty())
                addGraphDiagnostic(
                    diagnostics, ParticleModuleGraphDiagnosticSeverity::Error, "graph link has empty id");
            else if (hasDuplicateString(linkIds, link.id))
                addGraphDiagnostic(diagnostics,
                                   ParticleModuleGraphDiagnosticSeverity::Error,
                                   "graph link id is duplicated: " + link.id);
            linkIds.push_back(link.id);

            const ParticleGraphNode* source = findParticleGraphNode(emitter.graph, link.fromNodeId);
            const ParticleGraphNode* target = findParticleGraphNode(emitter.graph, link.toNodeId);
            if (source == nullptr || target == nullptr)
            {
                addGraphDiagnostic(
                    diagnostics, ParticleModuleGraphDiagnosticSeverity::Error, "graph link endpoint is missing");
                continue;
            }

            const ParticleModuleDefinition* sourceDefinition = findParticleModuleDefinition(source->typeId);
            const ParticleModuleDefinition* targetDefinition = findParticleModuleDefinition(target->typeId);
            const ParticleModulePort*       output =
                sourceDefinition == nullptr ? nullptr : findOutputPort(*sourceDefinition, link.fromPortId);
            const ParticleModulePort* input =
                targetDefinition == nullptr ? nullptr : findInputPort(*targetDefinition, link.toPortId);
            if (output == nullptr || input == nullptr)
            {
                addGraphDiagnostic(diagnostics,
                                   ParticleModuleGraphDiagnosticSeverity::Error,
                                   "graph link references an unknown port",
                                   target->typeId,
                                   target->moduleStableId);
            }
            else if (output->type != input->type)
            {
                addGraphDiagnostic(diagnostics,
                                   ParticleModuleGraphDiagnosticSeverity::Error,
                                   "graph link port types do not match",
                                   target->typeId,
                                   target->moduleStableId);
            }
        }

        for (const ParticleGraphNode& node : emitter.graph.nodes)
        {
            const ParticleModuleDefinition* definition = findParticleModuleDefinition(node.typeId);
            if (definition == nullptr)
                continue;

            for (const ParticleModuleDependency& dependencyInfo : definition->dependencies)
            {
                if (!dependencyInfo.required || dependencyInfo.outputId.empty())
                    continue;
                const ParticleGraphNode* source = findParticleGraphNodeForType(emitter.graph, dependencyInfo.typeId);
                if (source == nullptr ||
                    findParticleGraphLink(emitter.graph,
                                          source->id,
                                          dependencyInfo.outputId,
                                          node.id,
                                          dependencyInfo.outputId) == nullptr)
                {
                    addGraphDiagnostic(diagnostics,
                                       ParticleModuleGraphDiagnosticSeverity::Error,
                                       "required graph dependency link is missing: " + dependencyInfo.typeId,
                                       node.typeId,
                                       node.moduleStableId);
                }
            }
        }

        return !graphDiagnosticsHaveErrors(*diagnostics);
    }
} // namespace gts::particles
