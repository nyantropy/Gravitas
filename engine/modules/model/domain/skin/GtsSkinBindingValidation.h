#pragma once

#include <string>
#include <vector>

struct GtsSkinBinding;
struct GtsSkeletonAsset;

struct GtsSkinBindingValidationError
{
    std::string code;
    std::string message;
    std::string location;
};

struct GtsSkinBindingValidationResult
{
    std::vector<GtsSkinBindingValidationError> diagnostics;

    bool isValid() const
    {
        return diagnostics.empty();
    }
};

// Checks the stored expectation and binding structure, not remap bounds or mesh data.
[[nodiscard]] GtsSkinBindingValidationResult validateGtsSkinBinding(const GtsSkinBinding& binding);

// Also validates the supplied skeleton, exact compatibility, and remap bounds.
// Neither overload mutates its inputs or computes skin matrices.
[[nodiscard]] GtsSkinBindingValidationResult validateGtsSkinBinding(const GtsSkinBinding&   binding,
                                                                    const GtsSkeletonAsset& skeleton);
