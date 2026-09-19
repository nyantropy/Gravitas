#pragma once
#include "assets/cooking/AssetCooker.h"
#include "model/serialization/ModelAssetTypes.h"
struct GtsModelAsset;
struct GtsModelImportBundle;
namespace gts::rendering
{
    struct GtsModelCookerOptions : AssetCookerOptions
    {
        std::filesystem::path baseColorTextureOverride;
        bool                  vertexColorOnly = false;
    };
    struct GtsModelCookResult : AssetCookResult
    {
        std::vector<ModelAssetData> models;
    };
    class GtsModelCooker
    {
        public:
        static GtsModelCookResult cookModelBundle(const GtsModelImportBundle&  bundle,
                                                  const std::filesystem::path& sourcePath,
                                                  const GtsModelCookerOptions& options);

        static GtsModelCookResult cookModelAsset(const GtsModelAsset&         model,
                                                 const std::filesystem::path& sourcePath,
                                                 const GtsModelCookerOptions& options);

        static GtsModelCookResult cookSourceAsset(const std::filesystem::path& sourcePath,
                                                  const GtsModelCookerOptions& options);
    };
} // namespace gts::rendering
