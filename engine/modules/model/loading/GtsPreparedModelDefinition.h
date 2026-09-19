#pragma once

#include <filesystem>
#include <cstdint>
#include <vector>

#include "AssetTypes.h"
#include "GtsModelAsset.h"

// CPU definition supplied by cooked static content. Vertices remain prepared;
// material/dependency references remain unresolved until realization.
struct GtsPreparedModelDefinition
{
    std::filesystem::path                       referenceDirectory;
    std::vector<std::filesystem::path>          meshReferenceDirectories;
    std::vector<GtsModelNode>                   nodes;
    std::vector<uint32_t>                       rootNodes;
    std::vector<gts::rendering::MeshAssetData>  meshes;
    std::vector<gts::rendering::AssetReference> meshReferences;
    std::vector<gts::rendering::AssetReference> materials;
    std::vector<gts::rendering::AssetReference> dependencies;
};
