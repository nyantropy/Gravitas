#pragma once

#include <optional>
#include <vector>

#include "GtsModelAsset.h"
#include "GtsModelDiagnostic.h"

// carries the imported model and diagnostics, letting us assess success or failure, fairly simple
class GtsModelImportResult
{
public:
    // success validates the entire asset, while errors discard it, and warnings retain it
    static GtsModelImportResult success(GtsModelAsset asset,
                                        std::vector<GtsModelDiagnostic> diagnostics = {});
    // failure always contains an error and never exposes a partial asset
    static GtsModelImportResult failure(std::vector<GtsModelDiagnostic> diagnostics);

    bool succeeded() const { return model.has_value(); }
    bool hasWarnings() const;
    const GtsModelAsset* asset() const { return model ? &*model : nullptr; }
    const std::vector<GtsModelDiagnostic>& diagnostics() const { return messages; }

private:
    GtsModelImportResult(std::optional<GtsModelAsset> asset,
                        std::vector<GtsModelDiagnostic> diagnostics);

    std::optional<GtsModelAsset> model;
    std::vector<GtsModelDiagnostic> messages;
};
