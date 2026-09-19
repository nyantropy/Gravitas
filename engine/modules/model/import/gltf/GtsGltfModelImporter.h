#pragma once

#include "IGtsModelImporter.h"

class GtsGltfModelImporter final : public IGtsModelImporter
{
    public:
    [[nodiscard]] GtsModelImportResult importAsset(const GtsModelImportRequest& request) const override;
};
