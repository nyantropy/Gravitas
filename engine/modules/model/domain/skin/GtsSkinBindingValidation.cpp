#include "GtsSkinBindingValidation.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>

#include "GtsSkinBinding.h"
#include "model/domain/skeleton/GtsSkeletonAsset.h"
#include "model/domain/skeleton/GtsSkeletonValidation.h"

namespace
{
    void appendSkeletonErrors(const GtsSkeletonValidationResult& validation,
                              const char*                        code,
                              const char*                        location,
                              GtsSkinBindingValidationResult&    result)
    {
        for (const auto& error : validation.diagnostics)
        {
            result.diagnostics.push_back(
                {code, error.code + ": " + error.message, std::string(location) + "." + error.location});
        }
    }
} // namespace

GtsSkinBindingValidationResult validateGtsSkinBinding(const GtsSkinBinding& binding)
{
    GtsSkinBindingValidationResult result;
    appendSkeletonErrors(validateGtsSkeletonCompatibility(binding.targetSkeletonCompatibility),
                         "SKIN_TARGET_INVALID",
                         "targetSkeletonCompatibility",
                         result);

    if (binding.joints.empty())
        result.diagnostics.push_back(
            {"SKIN_JOINTS_EMPTY", "A binding requires at least one local joint slot.", "joints"});
    if (binding.joints.size() > std::numeric_limits<uint32_t>::max())
        result.diagnostics.push_back(
            {"SKIN_JOINT_COUNT", "Joint slot count exceeds the 32-bit index contract.", "joints"});

    for (size_t i = 0; i < binding.joints.size(); ++i)
    {
        const auto& joint    = binding.joints[i];
        const auto  location = "joints[" + std::to_string(i) + "]";
        if (joint.skeletonNodeIndex == GtsSkinJointBinding::InvalidSkeletonNodeIndex)
            result.diagnostics.push_back({"SKIN_NODE_UNASSIGNED",
                                          "A local slot requires an explicit skeleton node mapping.",
                                          location + ".skeletonNodeIndex"});

        const auto& matrix = joint.inverseBindMatrix;
        bool        finite = true;
        for (glm::length_t column = 0; column < 4; ++column)
            for (glm::length_t row = 0; row < 4; ++row)
                finite = std::isfinite(matrix[column][row]) && finite;
        if (!finite)
            result.diagnostics.push_back({"SKIN_INVERSE_BIND_NONFINITE",
                                          "Inverse-bind matrix elements must be finite.",
                                          location + ".inverseBindMatrix"});
        else if (matrix[0][3] != 0.0f || matrix[1][3] != 0.0f || matrix[2][3] != 0.0f || matrix[3][3] != 1.0f)
            result.diagnostics.push_back({"SKIN_INVERSE_BIND_NOT_AFFINE",
                                          "Inverse-bind matrix bottom row must be [0, 0, 0, 1].",
                                          location + ".inverseBindMatrix"});
    }
    return result;
}

GtsSkinBindingValidationResult validateGtsSkinBinding(const GtsSkinBinding& binding, const GtsSkeletonAsset& skeleton)
{
    auto       result  = validateGtsSkinBinding(binding);
    const auto derived = makeGtsSkeletonCompatibility(skeleton);
    for (const auto& error : derived.diagnostics())
        result.diagnostics.push_back(
            {"SKIN_SKELETON_INVALID", error.code + ": " + error.message, "skeleton." + error.location});
    // Structural errors are already reported with their locations. Compare only
    // valid contracts so malformed expectations are not called mismatches.
    if (derived.succeeded() && validateGtsSkeletonCompatibility(binding.targetSkeletonCompatibility).isValid() &&
        !areGtsSkeletonCompatibilitiesEqual(binding.targetSkeletonCompatibility, *derived.compatibility()))
        result.diagnostics.push_back(
            {"SKIN_SKELETON_INCOMPATIBLE", "Supplied skeleton does not match the exact target contract.", "skeleton"});

    for (size_t i = 0; i < binding.joints.size(); ++i)
    {
        const auto index = binding.joints[i].skeletonNodeIndex;
        if (index != GtsSkinJointBinding::InvalidSkeletonNodeIndex && index >= skeleton.nodes.size())
            result.diagnostics.push_back({"SKIN_NODE_OUT_OF_RANGE",
                                          "Local slot mapping does not reference a supplied skeleton node.",
                                          "joints[" + std::to_string(i) + "].skeletonNodeIndex"});
    }
    return result;
}
