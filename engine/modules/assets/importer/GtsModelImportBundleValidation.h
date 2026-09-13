#pragma once

#include <vector>

#include "assets/model/GtsModelDiagnostic.h"

struct GtsModelImportBundle;

struct GtsModelImportBundleValidationResult
{
    std::vector<GtsModelDiagnostic> diagnostics;

    bool isValid() const
    {
        return diagnostics.empty();
    }
};

// requires at least one product - all model uses currently share a listed
// definition and its ownership, while external-reference linking is deferred
[[nodiscard]] GtsModelImportBundleValidationResult validateGtsModelImportBundle(const GtsModelImportBundle& bundle);
