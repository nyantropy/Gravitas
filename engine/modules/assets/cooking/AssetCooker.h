#pragma once

#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

#include "AssetTypes.h"
#include "MeshAssetGeometry.h"

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
        std::string           explicitImporter;
        TextureCookRole       textureRole            = TextureCookRole::BaseColor;
        bool                  generateTextureMipmaps = true;
    };

    struct AssetCookResult
    {
        std::vector<MeshAssetData>     meshes;
        std::vector<MaterialAssetData> materials;
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
        static AssetCookResult cookSourceAsset(const std::filesystem::path& sourcePath,
                                               const AssetCookerOptions&    options);
    };
} // namespace gts::rendering
