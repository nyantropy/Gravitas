#pragma once

#include <vector>

#include "GtsModelDiagnostic.h"

struct GtsModelAsset;
struct GtsModelPrimitive;
struct GtsModelMaterial;

struct GtsModelValidationResult
{
    std::vector<GtsModelDiagnostic> diagnostics;

    bool isValid() const { return diagnostics.empty(); }
};

// missing streams must be omitted; present streams must be nonempty, correctly
// typed, finite, uniquely keyed, and match Position[0]s vertex count
// primitive validation checks geometry; asset validation also checks references
// and requires every node to belong to exactly one rooted, acyclic tree
[[nodiscard]] GtsModelValidationResult validateGtsModelPrimitive(const GtsModelPrimitive& primitive);
// checks appearance values and enums; image bounds and UV availability need the asset
[[nodiscard]] GtsModelValidationResult validateGtsModelMaterial(const GtsModelMaterial& material);
[[nodiscard]] GtsModelValidationResult validateGtsModelAsset(const GtsModelAsset& asset);
