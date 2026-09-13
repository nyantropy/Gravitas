#pragma once

struct GtsModelAsset;
struct GtsModelPrimitive;
struct GtsSkinBinding;
struct GtsModelValidationResult;

// internal phase of validateGtsModelAsset; generic primitive validation runs first
namespace gtsModelValidationDetail
{
    void validateSkinAssociations(const GtsModelAsset& asset, GtsModelValidationResult& result);
}

// Generic geometry plus bound influence validation. Binding validity is a separate skin-domain check.
[[nodiscard]] GtsModelValidationResult validateGtsModelPrimitiveSkin(const GtsModelPrimitive& primitive,
                                                                   const GtsSkinBinding& binding);
