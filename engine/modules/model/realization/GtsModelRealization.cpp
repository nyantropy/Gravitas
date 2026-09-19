#include "GtsModelRealization.h"

#include <map>
#include <tuple>

#include "model/loading/GtsModelResource.h"
#include "model/loading/GtsPreparedModelDefinition.h"
#include "model/processing/geometry/static/GtsStaticMeshPreparation.h"
#include "model/processing/geometry/skinned/GtsSkinnedMeshPreparation.h"

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
    return {std::move(result), std::move(diagnostics)};
}
