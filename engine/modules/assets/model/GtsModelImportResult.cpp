#include "GtsModelImportResult.h"

#include <algorithm>
#include <iterator>
#include <utility>

#include "GtsModelValidation.h"

namespace
{
    bool hasErrors(const std::vector<GtsModelDiagnostic>& diagnostics)
    {
        return std::any_of(diagnostics.begin(), diagnostics.end(), [](const GtsModelDiagnostic& diagnostic)
        {
            return diagnostic.severity == GtsModelDiagnosticSeverity::Error;
        });
    }
}

GtsModelImportResult::GtsModelImportResult(std::optional<GtsModelAsset> asset,
                                         std::vector<GtsModelDiagnostic> diagnostics)
    : model(std::move(asset)), messages(std::move(diagnostics))
{
}

GtsModelImportResult GtsModelImportResult::success(GtsModelAsset asset,
                                                 std::vector<GtsModelDiagnostic> diagnostics)
{
    GtsModelValidationResult validation = validateGtsModelAsset(asset);
    diagnostics.insert(diagnostics.end(),
                       std::make_move_iterator(validation.diagnostics.begin()),
                       std::make_move_iterator(validation.diagnostics.end()));
    if (hasErrors(diagnostics))
    {
        return failure(std::move(diagnostics));
    }
    return GtsModelImportResult(std::move(asset), std::move(diagnostics));
}

GtsModelImportResult GtsModelImportResult::failure(std::vector<GtsModelDiagnostic> diagnostics)
{
    if (!hasErrors(diagnostics))
    {
        diagnostics.push_back({GtsModelDiagnosticSeverity::Error, "MODEL_IMPORT_FAILED",
                               "Model import failed without an error diagnostic.", {}});
    }
    return GtsModelImportResult(std::nullopt, std::move(diagnostics));
}

bool GtsModelImportResult::hasWarnings() const
{
    return std::any_of(messages.begin(), messages.end(), [](const GtsModelDiagnostic& diagnostic)
    {
        return diagnostic.severity == GtsModelDiagnosticSeverity::Warning;
    });
}
