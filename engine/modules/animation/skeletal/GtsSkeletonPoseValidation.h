#pragma once

#include <string>
#include <vector>

struct GtsSkeletonPose;
struct GtsSkeletonAsset;

struct GtsSkeletonPoseValidationError
{
    std::string code;
    std::string message;
    std::string location;
};

struct GtsSkeletonPoseValidationResult
{
    std::vector<GtsSkeletonPoseValidationError> diagnostics;
    bool                                        isValid() const
    {
        return diagnostics.empty();
    }
};

// validates the exact index contract, array sizes and transform values without rebuilding the hierarchy
[[nodiscard]] GtsSkeletonPoseValidationResult validateGtsSkeletonPose(const GtsSkeletonPose&  pose,
                                                                      const GtsSkeletonAsset& skeleton);
