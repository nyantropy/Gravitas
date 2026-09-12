#pragma once

#include <filesystem>

// request is just a path
struct GtsModelImportRequest
{
    std::filesystem::path sourcePath;
};

class GtsModelImportResult;

// source files -> importer -> validated GtsModelAsset -> future runtime realization
// parser types and format-specific decisions stop at this boundary
class IGtsModelImporter
{
public:
    virtual ~IGtsModelImporter() = default;
    [[nodiscard]] virtual GtsModelImportResult importAsset(const GtsModelImportRequest& request) const = 0;
};
