#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "assets/serialization/AssetTypes.h"
#include "assets/loading/RuntimeAssetPolicy.h"
#include "model/domain/model/GtsModelDiagnostic.h"

namespace gts::rendering
{
    // cpu loading phase shared by runtime resource creation and headless tests
    // basically this loads the cooked asset if it is available, if not, we cook it by hauling it through the whole pipeline
    std::filesystem::path resolveRuntimeMeshPath(const std::filesystem::path& requestedPath);
    MeshAssetData loadRuntimeMeshAsset(const std::filesystem::path& requestedPath,
                                      std::vector<GtsModelDiagnostic>& diagnostics);
}
