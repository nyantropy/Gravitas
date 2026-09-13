#pragma once

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "GtsSkeletonPose.h"

struct GtsSkeletonAsset;
struct GtsAnimationClipAsset;

struct GtsSkeletonPoseEvaluationError
{
    std::string code;
    std::string message;
    std::string location;
};

class GtsSkeletonPoseEvaluationResult
{
    public:
    bool succeeded() const
    {
        return evaluatedPose.has_value();
    }
    const GtsSkeletonPose* pose() const
    {
        return evaluatedPose ? &*evaluatedPose : nullptr;
    }
    const std::vector<GtsSkeletonPoseEvaluationError>& diagnostics() const
    {
        return errors;
    }

    private:
    friend GtsSkeletonPoseEvaluationResult evaluateGtsDefaultPose(const GtsSkeletonAsset&);
    friend GtsSkeletonPoseEvaluationResult
    evaluateGtsAnimationPose(const GtsSkeletonAsset&, const GtsAnimationClipAsset&, float);
    GtsSkeletonPoseEvaluationResult(std::optional<GtsSkeletonPose>              pose,
                                    std::vector<GtsSkeletonPoseEvaluationError> diagnostics)
        : evaluatedPose(std::move(pose)), errors(std::move(diagnostics))
    {
    }

    std::optional<GtsSkeletonPose>              evaluatedPose;
    std::vector<GtsSkeletonPoseEvaluationError> errors;
};

[[nodiscard]] GtsSkeletonPoseEvaluationResult evaluateGtsDefaultPose(const GtsSkeletonAsset& skeleton);

// finite time within inclusive [0, duration]; no looping, wrapping or clip-time clamping
[[nodiscard]] GtsSkeletonPoseEvaluationResult
evaluateGtsAnimationPose(const GtsSkeletonAsset& skeleton, const GtsAnimationClipAsset& clip, float timeSeconds);
