#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "assets/serialization/AssetTypes.h"

namespace gts::rendering
{
    // CPU image input for standalone and model texture cooking; never a model import DTO.
    enum class TextureCookSource
    {
        ExternalFile,
        EmbeddedBytes
    };

    struct TextureCookInput
    {
        std::string           debugName;
        std::filesystem::path sourcePath;
        std::string           logicalPath;
        std::vector<uint8_t>  embeddedBytes;
        std::vector<uint8_t>  rgba8Pixels;
        uint32_t              width              = 0;
        uint32_t              height             = 0;
        uint32_t              sourceChannelCount = 0;
        TextureCookSource     source             = TextureCookSource::ExternalFile;

        bool decoded() const
        {
            return width > 0 && height > 0 && !rgba8Pixels.empty();
        }
    };

    struct TextureCookerOptions
    {
        TextureCookRole    role            = TextureCookRole::BaseColor;
        bool               generateMipmaps = true;
        TextureSamplerDesc sampler{};
        std::string        debugName;
    };

    struct TextureCookResult
    {
        TextureAssetData             texture;
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

    TextureColorSpace  colorSpaceForTextureCookRole(TextureCookRole role);
    const char*        textureCookRoleName(TextureCookRole role);
    bool               parseTextureCookRole(const std::string& value, TextureCookRole& role);
    TextureSamplerDesc defaultSamplerForTextureCookRole(TextureCookRole role, bool generateMipmaps);

    class TextureCooker
    {
        public:
        static TextureCookResult cookTexture(const TextureCookInput&      imported,
                                             AssetId                      assetId,
                                             const TextureCookerOptions&  options,
                                             const std::filesystem::path& diagnosticSource = {});
    };
} // namespace gts::rendering
