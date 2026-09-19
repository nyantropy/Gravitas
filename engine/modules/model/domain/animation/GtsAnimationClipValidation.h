#pragma once

#include <string>
#include <vector>

struct GtsAnimationClipAsset;
struct GtsSkeletonAsset;

struct GtsAnimationClipValidationError
{
    std::string code;
    std::string message;
    std::string location;
};

struct GtsAnimationClipValidationResult
{
    std::vector<GtsAnimationClipValidationError> diagnostics;

    bool isValid() const
    {
        return diagnostics.empty();
    }
};

// validates the self-contained contract and all key data without repairing input
[[nodiscard]] GtsAnimationClipValidationResult validateGtsAnimationClip(const GtsAnimationClipAsset& clip);

// also validates a supplied definition and requires exact structural compatibility
[[nodiscard]] GtsAnimationClipValidationResult validateGtsAnimationClip(const GtsAnimationClipAsset& clip,
                                                                        const GtsSkeletonAsset&      skeleton);
