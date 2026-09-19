#pragma once

#include <optional>
#include <vector>

#include "GtsModelImportBundle.h"
#include "GtsModelDiagnostic.h"

// carries the imported model and diagnostics, letting us assess success or failure, fairly simple
class GtsModelImportResult
{
public:
    // success validates the entire asset, while errors discard it, and warnings retain it
    static GtsModelImportResult success(GtsModelAsset asset,
                                        std::vector<GtsModelDiagnostic> diagnostics = {});
    static GtsModelImportResult success(GtsModelImportBundle bundle,
                                        std::vector<GtsModelDiagnostic> diagnostics = {});
    // failure always contains an error and never exposes a partial asset
    static GtsModelImportResult failure(std::vector<GtsModelDiagnostic> diagnostics);

    bool succeeded() const { return importedAssets.has_value(); }
    bool hasWarnings() const;
    const GtsModelImportBundle* bundle() const { return importedAssets ? &*importedAssets : nullptr; }
    // A successful associated-asset import may have no primary model.
    const GtsModelAsset* asset() const { return importedAssets && importedAssets->model ? &*importedAssets->model : nullptr; }
    const std::vector<GtsModelDiagnostic>& diagnostics() const { return messages; }

private:
    GtsModelImportResult(std::optional<GtsModelImportBundle> bundle,
                        std::vector<GtsModelDiagnostic> diagnostics);

    std::optional<GtsModelImportBundle> importedAssets;
    std::vector<GtsModelDiagnostic> messages;
};
