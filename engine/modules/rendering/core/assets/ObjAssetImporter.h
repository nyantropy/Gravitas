#pragma once

#include "AssetImporter.h"

namespace gts::rendering
{
    class ObjAssetImporter : public IAssetImporter
    {
    public:
        std::string_view name() const override;
        uint32_t version() const override;
        AssetImportCapability capabilities() const override;
        bool supports(const std::filesystem::path& sourcePath) const override;
        AssetImportResult importAsset(const AssetImportRequest& request) const override;
    };
}
