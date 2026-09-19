#include "GtsSkinPaletteEvaluation.h"

#include <cmath>

#include "animation/skeletal/GtsSkeletonPose.h"
#include "animation/skeletal/GtsSkeletonPoseValidation.h"
#include "model/domain/skin/GtsSkinBinding.h"
#include "model/domain/skin/GtsSkinBindingValidation.h"

GtsSkinPaletteEvaluationResult
evaluateGtsSkinPalette(const GtsSkeletonPose& pose, const GtsSkinBinding& binding, const GtsSkeletonAsset& skeleton)
{
    std::vector<GtsSkinPaletteEvaluationError> errors;
    for (const auto& error : validateGtsSkinBinding(binding, skeleton).diagnostics)
        errors.push_back({error.code, error.message, "binding." + error.location});
    for (const auto& error : validateGtsSkeletonPose(pose, skeleton).diagnostics)
        errors.push_back({error.code, error.message, error.location});
    if (!errors.empty())
        return {std::nullopt, std::move(errors)};

    GtsSkinPalette palette;
    palette.matrices.reserve(binding.joints.size());
    for (size_t slot = 0; slot < binding.joints.size(); ++slot)
    {
        const auto&     joint  = binding.joints[slot];
        const glm::mat4 matrix = pose.modelTransforms[joint.skeletonNodeIndex] * joint.inverseBindMatrix;
        for (glm::length_t c = 0; c < 4; ++c)
            for (glm::length_t r = 0; r < 4; ++r)
                if (!std::isfinite(matrix[c][r]))
                    return {std::nullopt,
                            {{"SKIN_PALETTE_NONFINITE",
                              "Palette multiplication exceeded finite matrix storage.",
                              "slots[" + std::to_string(slot) + "].skeletonNodes[" +
                                  std::to_string(joint.skeletonNodeIndex) + "]"}}};
        palette.matrices.push_back(matrix);
    }
    return {std::move(palette), {}};
}
