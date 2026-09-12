#pragma once

#include "assets/importer/IGtsModelImporter.h"

class GtsObjModelImporter final : public IGtsModelImporter
{
    public:
    [[nodiscard]] GtsModelImportResult importAsset(const GtsModelImportRequest& request) const override;
};
