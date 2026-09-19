#pragma once

#include "assets/serialization/AssetTypes.h"

namespace gts::rendering
{
    struct ModelNodeAssetData
    {
        std::string    name;
        int32_t        parentIndex = -1;
        AssetReference mesh;
        glm::mat4      localTransform = glm::mat4(1.0f);
    };

    struct ModelAssetData
    {
        AssetId                         id = InvalidAssetId;
        std::string                     debugName;
        std::vector<ModelNodeAssetData> nodes;
        std::vector<AssetReference>     meshes;
        std::vector<AssetReference>     materials;
        std::vector<AssetReference>     dependencies;
    };
} // namespace gts::rendering
