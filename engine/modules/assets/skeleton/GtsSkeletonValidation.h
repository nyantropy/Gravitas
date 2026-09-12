#pragma once

#include <string>
#include <vector>

struct GtsSkeletonAsset;

// validation reports errors only, in deterministic node/field order
struct GtsSkeletonValidationError
{
    std::string code;
    std::string message;
    std::string location;
};

struct GtsSkeletonValidationResult
{
    std::vector<GtsSkeletonValidationError> diagnostics;

    bool isValid() const
    {
        return diagnostics.empty();
    }
};

// nonempty forest; every parent precedes its child, and validation never normalizes,
// decomposes, reorders, or otherwise mutates the asset
[[nodiscard]] GtsSkeletonValidationResult validateGtsSkeletonAsset(const GtsSkeletonAsset& skeleton);
