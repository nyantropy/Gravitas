#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "assets/model/GtsModelDiagnostic.h"
#include "assets/skeleton/GtsSkeletonTypes.h"

struct GtsModelAsset;
struct GtsSkeletonAsset;

namespace gts::gltf
{
    struct SourceDocument;

    // source-node indices remain intact here; caller remaps correspondence
    // together with model nodes after selected-scene pruning
    std::vector<std::shared_ptr<const GtsSkeletonAsset>>
    importSkins(const SourceDocument&                         data,
                GtsModelAsset&                                model,
                const std::vector<GtsSkeletonLocalTransform>& localTransforms,
                const std::vector<uint32_t>&                  nodeSkins,
                const std::vector<bool>&                      active,
                std::vector<GtsModelDiagnostic>&              diagnostics);
} // namespace gts::gltf
