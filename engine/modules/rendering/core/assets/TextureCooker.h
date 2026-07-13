#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "AssetTypes.h"

namespace gts::rendering
{
    struct TextureCookerOptions
    {
        TextureCookRole role = TextureCookRole::BaseColor;
        bool generateMipmaps = true;
        TextureSamplerDesc sampler{};
        std::string debugName;
    };

    struct TextureCookResult
    {
        TextureAssetData texture;
        std::vector<AssetDiagnostic> diagnostics;

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

    TextureColorSpace colorSpaceForTextureCookRole(TextureCookRole role);
    const char* textureCookRoleName(TextureCookRole role);
    bool parseTextureCookRole(const std::string& value, TextureCookRole& role);
    TextureSamplerDesc defaultSamplerForTextureCookRole(TextureCookRole role, bool generateMipmaps);

    class TextureCooker
    {
    public:
        static TextureCookResult cookImportedTexture(const ImportedTexture& imported,
                                                     AssetId assetId,
                                                     const TextureCookerOptions& options,
                                                     const std::filesystem::path& diagnosticSource = {});
    };
}
