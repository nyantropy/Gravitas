#pragma once

#include <optional>
#include <vector>

#include "AssetTypes.h"
#include "assets/model/GtsModelDiagnostic.h"

struct GtsModelAsset;

namespace gts::rendering
{
    // single-resource v1 realization accepts only one identity root per mesh
    // empty material references leave source materials unresolved (development loading)
    // this basically converts meshes into a cpu structure for cooked storage
    std::optional<MeshAssetData> realizeGtsFlatStaticModel(
        const GtsModelAsset& model,
        std::vector<GtsModelDiagnostic>& diagnostics,
        const std::vector<AssetReference>& materials = {},
        const AssetReference& defaultMaterial = {});
}
