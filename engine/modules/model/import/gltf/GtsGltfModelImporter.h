#pragma once

#include "model/import/IGtsModelImporter.h"

class GtsGltfModelImporter final : public IGtsModelImporter
{
    public:
    [[nodiscard]] GtsModelImportResult importAsset(const GtsModelImportRequest& request) const override;
};
