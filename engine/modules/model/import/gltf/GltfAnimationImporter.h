#pragma once

#include <vector>

#include "GtsAnimationClipAsset.h"

namespace gts::gltf
{
    struct SourceDocument;
    struct GltfSkinImportResult;

    std::vector<GtsAnimationClipAsset> importAnimations(const SourceDocument& data, const GltfSkinImportResult& skins);
} // namespace gts::gltf
