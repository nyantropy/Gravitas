#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "model/domain/model/GtsModelDiagnostic.h"
#include "model/domain/skeleton/GtsSkeletonTypes.h"

struct GtsModelAsset;
struct GtsSkeletonAsset;

namespace gts::gltf
{
    struct SourceDocument;

    struct GltfSkinImportResult
    {
        std::vector<std::shared_ptr<const GtsSkeletonAsset>> skeletons;
        // per definition, evaluation-node index -> original source node index
        // survives scene pruning; not a canonical identity or occurrence table
        std::vector<std::vector<uint32_t>> sourceNodes;
    };

    // source-node indices remain intact here; caller remaps correspondence
    // together with model nodes after selected-scene pruning
    GltfSkinImportResult importSkins(const SourceDocument&                         data,
                                     GtsModelAsset&                                model,
                                     const std::vector<GtsSkeletonLocalTransform>& localTransforms,
                                     const std::vector<uint32_t>&                  nodeSkins,
                                     const std::vector<bool>&                      active,
                                     std::vector<GtsModelDiagnostic>&              diagnostics);
} // namespace gts::gltf
