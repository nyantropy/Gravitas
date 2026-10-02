#include "GtsModelRealization.h"

#include <cmath>
#include <stdexcept>
#include <map>
#include <tuple>

#include "GtsModelResource.h"
#include "GtsModelAsset.h"
#include "GtsPreparedModelDefinition.h"
#include "GtsStaticMeshPreparation.h"
#include "GtsSkinnedMeshPreparation.h"

namespace
{
    std::vector<glm::mat4> prepareAuthoredNodeTransforms(const GtsModelResource& model)
    {
        const auto             nodes = model.nodes();
        std::vector<glm::mat4> transforms(nodes.size(), glm::mat4(1));
        std::vector<bool>      visited(nodes.size());
        struct Pending
        {
            uint32_t  node;
            glm::mat4 parent;
        };
        std::vector<Pending> pending;
        for (auto root : model.rootNodes())
            pending.push_back({root, glm::mat4(1)});
        while (!pending.empty())
        {
            const auto item = pending.back();
            pending.pop_back();
            if (item.node >= nodes.size() || visited[item.node])
                throw std::runtime_error("Invalid model hierarchy at node " + std::to_string(item.node));
            visited[item.node]    = true;
            transforms[item.node] = item.parent * nodes[item.node].localTransform;
            for (int column = 0; column < 4; ++column)
                for (int row = 0; row < 4; ++row)
                    if (!std::isfinite(transforms[item.node][column][row]))
                        throw std::runtime_error("Non-finite model transform at node " + std::to_string(item.node));
            for (auto child : nodes[item.node].children)
                pending.push_back({child, transforms[item.node]});
        }
        for (uint32_t i = 0; i < nodes.size(); ++i)
            if (!visited[i])
                throw std::runtime_error("Unreachable model node " + std::to_string(i));
        return transforms;
    }
} // namespace

GtsModelRealizationResult realizeGtsModel(GtsModelHandle model)
{
    if (!model)
        return {nullptr,
                {{GtsModelDiagnosticSeverity::Error, "model.realization.missing", "Model handle is empty", {}}}};

    auto result   = std::make_shared<GtsRealizedModel>();
    result->model = model;
    std::vector<GtsModelDiagnostic> diagnostics;
    // First occurrence defines output order; ordered lookup is never used for traversal.
    using Key = std::tuple<uint32_t, std::optional<uint32_t>>;
    std::map<Key, uint32_t> definitions;
    const auto*             canonical = model->canonicalModel();
    const auto*             cooked    = model->preparedModel();
    for (uint32_t nodeIndex = 0; nodeIndex < model->nodes().size(); ++nodeIndex)
    {
        const auto& node = model->nodes()[nodeIndex];
        if (!node.meshIndex)
            continue;
        const auto meshIndex = *node.meshIndex;
        const Key  key{meshIndex, node.skinBindingIndex};
        auto       found = definitions.find(key);
        if (found == definitions.end())
        {
            const auto context =
                model->identityPath().string() + ": nodes[" + std::to_string(nodeIndex) + "] '" + node.name +
                "', meshes[" + std::to_string(meshIndex) + "]" +
                (node.skinBindingIndex ? ", skinBindings[" + std::to_string(*node.skinBindingIndex) + "]" : "");
            auto appendDiagnostics = [&](const auto& preparation)
            {
                for (auto message : preparation.diagnostics())
                {
                    message.location = context + ": " + message.location;
                    diagnostics.push_back(std::move(message));
                }
            };
            if (canonical)
            {
                if (node.skinBindingIndex)
                {
                    const auto prepared = prepareGtsSkinnedMesh(
                        canonical->meshes[meshIndex], canonical->skinBindings[*node.skinBindingIndex].binding);
                    appendDiagnostics(prepared);
                    if (!prepared.succeeded())
                        return {nullptr, std::move(diagnostics)};
                    result->geometry.emplace_back(std::make_shared<const GtsPreparedSkinnedMesh>(*prepared.mesh()));
                }
                else
                {
                    const auto prepared = prepareGtsStaticMesh(canonical->meshes[meshIndex]);
                    appendDiagnostics(prepared);
                    if (!prepared.succeeded())
                        return {nullptr, std::move(diagnostics)};
                    result->geometry.emplace_back(std::make_shared<const GtsPreparedStaticMesh>(*prepared.mesh()));
                }
            }
            else
            {
                // Aliasing shared ownership keeps the original cooked arrays alive, without copying.
                result->geometry.emplace_back(
                    std::shared_ptr<const gts::rendering::MeshAssetData>(model, &cooked->meshes[meshIndex]),
                    cooked->meshReferenceDirectories[meshIndex]);
            }
            found = definitions.emplace(key, static_cast<uint32_t>(result->geometry.size() - 1)).first;
        }
        std::optional<uint32_t> skeletonUse;
        if (node.skinBindingIndex)
            skeletonUse = canonical->skinBindings[*node.skinBindingIndex].skeletonUseIndex;
        result->occurrences.push_back({nodeIndex, found->second, node.skinBindingIndex, skeletonUse});
    }
    if (result->occurrences.empty())
        return {nullptr,
                {{GtsModelDiagnosticSeverity::Error,
                  "model.realization.empty",
                  "Model has no mesh occurrences",
                  model->identityPath().string()}}};
    try
    {
        result->authoredNodeTransforms = prepareAuthoredNodeTransforms(*model);
    }
    catch (const std::runtime_error& error)
    {
        result->authoredNodeTransforms = std::string(error.what());
    }
    return {std::move(result), std::move(diagnostics)};
}
