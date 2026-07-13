#pragma once

#include "AssetImporter.h"

namespace gts::rendering
{
    class ImageAssetImporter final : public IAssetImporter
    {
    public:
        std::string_view name() const override;
        uint32_t version() const override;
        AssetImportCapability capabilities() const override;
        bool supports(const std::filesystem::path& sourcePath) const override;
        AssetImportResult importAsset(const AssetImportRequest& request) const override;

        static bool decodeFile(const std::filesystem::path& path,
                               ImportedTexture& texture,
                               std::string* error = nullptr);

        static bool decodeBytes(const std::vector<uint8_t>& bytes,
                                ImportedTexture& texture,
                                std::string* error = nullptr);
    };
}
