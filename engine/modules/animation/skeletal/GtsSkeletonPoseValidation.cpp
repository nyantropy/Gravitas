#include "GtsSkeletonPoseValidation.h"

#include <cmath>
#include <variant>

#include "GtsSkeletonPose.h"
#include "model/domain/skeleton/GtsSkeletonAsset.h"
#include "model/domain/skeleton/GtsSkeletonValidation.h"

namespace
{
    template <class Vector> bool finite(const Vector& value)
    {
        for (glm::length_t i = 0; i < value.length(); ++i)
            if (!std::isfinite(value[i]))
                return false;
        return true;
    }

    void validateMatrix(const glm::mat4& matrix, const std::string& location, GtsSkeletonPoseValidationResult& result)
    {
        for (glm::length_t c = 0; c < 4; ++c)
            if (!finite(matrix[c]))
            {
                result.diagnostics.push_back(
                    {"POSE_MATRIX_NONFINITE", "Pose matrix elements must be finite.", location});
                return;
            }
        if (matrix[0][3] != 0 || matrix[1][3] != 0 || matrix[2][3] != 0 || matrix[3][3] != 1)
            result.diagnostics.push_back(
                {"POSE_MATRIX_NOT_AFFINE", "Pose matrix bottom row must be [0, 0, 0, 1].", location});
    }
} // namespace

GtsSkeletonPoseValidationResult validateGtsSkeletonPose(const GtsSkeletonPose& pose, const GtsSkeletonAsset& skeleton)
{
    GtsSkeletonPoseValidationResult result;
    const auto                      expected = makeGtsSkeletonCompatibility(skeleton);
    for (const auto& error : expected.diagnostics())
        result.diagnostics.push_back({error.code, error.message, "skeleton." + error.location});
    const auto contractValidation = validateGtsSkeletonCompatibility(pose.skeletonCompatibility);
    for (const auto& error : contractValidation.diagnostics)
        result.diagnostics.push_back({error.code, error.message, "pose.skeletonCompatibility." + error.location});
    if (expected.succeeded() && contractValidation.isValid() &&
        !areGtsSkeletonCompatibilitiesEqual(pose.skeletonCompatibility, *expected.compatibility()))
        result.diagnostics.push_back({"POSE_SKELETON_INCOMPATIBLE",
                                      "Pose does not use the supplied skeleton's exact index contract.",
                                      "pose.skeletonCompatibility"});
    if (pose.localTransforms.size() != skeleton.nodes.size())
        result.diagnostics.push_back(
            {"POSE_LOCAL_COUNT", "Local transform count must equal skeleton node count.", "pose.localTransforms"});
    if (pose.modelTransforms.size() != skeleton.nodes.size())
        result.diagnostics.push_back(
            {"POSE_MODEL_COUNT", "Model transform count must equal skeleton node count.", "pose.modelTransforms"});

    for (size_t i = 0; i < pose.localTransforms.size(); ++i)
    {
        const auto& local    = pose.localTransforms[i];
        const auto  location = "pose.localTransforms[" + std::to_string(i) + "]";
        if (i < skeleton.nodes.size() && local.index() != skeleton.nodes[i].defaultLocalTransform.index())
            result.diagnostics.push_back(
                {"POSE_TRANSFORM_FORM", "Pose local transform must retain the skeleton's TRS/matrix form.", location});
        if (const auto* trs = std::get_if<GtsSkeletonTrs>(&local))
        {
            if (!finite(trs->translation) || !finite(trs->rotation) || !finite(trs->scale))
                result.diagnostics.push_back({"POSE_TRS_NONFINITE", "Pose TRS components must be finite.", location});
            else
            {
                double squaredLength = 0;
                for (glm::length_t c = 0; c < 4; ++c)
                    squaredLength += static_cast<double>(trs->rotation[c]) * trs->rotation[c];
                if (std::abs(squaredLength - 1) > 1e-4)
                    result.diagnostics.push_back(
                        {"POSE_ROTATION_NOT_UNIT",
                         "Pose rotations must have unit magnitude (squared-length tolerance 1e-4).",
                         location});
            }
        }
        else if (const auto* matrix = std::get_if<glm::mat4>(&local))
            validateMatrix(*matrix, location, result);
        else
            result.diagnostics.push_back(
                {"POSE_TRANSFORM_INVALID", "Pose local transform must contain TRS or a matrix.", location});
    }
    for (size_t i = 0; i < pose.modelTransforms.size(); ++i)
        validateMatrix(pose.modelTransforms[i], "pose.modelTransforms[" + std::to_string(i) + "]", result);
    return result;
}
