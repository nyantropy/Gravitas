#pragma once

struct GtsModelAsset;
struct GtsModelValidationResult;

// internal phase of validateGtsModelAsset; generic primitive validation runs first
namespace gtsModelValidationDetail
{
    void validateSkinAssociations(const GtsModelAsset& asset, GtsModelValidationResult& result);
}
