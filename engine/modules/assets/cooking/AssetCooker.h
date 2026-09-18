#pragma once

#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

#include "assets/serialization/AssetTypes.h"
#include "assets/serialization/MeshAssetGeometry.h"

struct GtsModelAsset;
struct GtsModelImportBundle;

namespace gts::rendering
{
    enum class CookedAssetOutputType
    {
        Mesh,
        Material,
        Model,
        Texture,
        TextureDependency
    };

    struct CookedAssetOutput
    {
        CookedAssetOutputType type = CookedAssetOutputType::Mesh;
        std::filesystem::path path;
        AssetReference        reference;
    };

    struct AssetCookerOptions
    {
        std::filesystem::path outputDirectory;
        std::filesystem::path baseColorTextureOverride;
        bool                  vertexColorOnly = false;
        std::string           explicitImporter;
        TextureCookRole       textureRole            = TextureCookRole::BaseColor;
        bool                  generateTextureMipmaps = true;
    };

    struct AssetCookResult
    {
        std::vector<MeshAssetData>     meshes;
        std::vector<MaterialAssetData> materials;
        std::vector<ModelAssetData>    models;
        std::vector<TextureAssetData>  textures;
        std::vector<CookedAssetOutput> outputs;
        std::vector<AssetDiagnostic>   diagnostics;

        bool hasErrors() const
        {
            for (const AssetDiagnostic& diagnostic : diagnostics)
            {
                if (diagnostic.severity == AssetDiagnosticSeverity::Error)
                    return true;
            }
            return false;
        }

        bool succeeded() const
        {
            return !hasErrors();
        }
    };

    class AssetCooker
    {
        public:
        static AssetCookResult cookModelBundle(const GtsModelImportBundle&  bundle,
                                               const std::filesystem::path& sourcePath,
                                               const AssetCookerOptions&    options);

        static AssetCookResult cookModelAsset(const GtsModelAsset&         model,
                                              const std::filesystem::path& sourcePath,
                                              const AssetCookerOptions&    options);

        static AssetCookResult cookSourceAsset(const std::filesystem::path& sourcePath,
                                               const AssetCookerOptions&    options);
    };
} // namespace gts::rendering
