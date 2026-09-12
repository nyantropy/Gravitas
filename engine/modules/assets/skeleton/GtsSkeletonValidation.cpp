#include "GtsSkeletonValidation.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <set>
#include <string_view>
#include <variant>

#include "GtsSkeletonAsset.h"

namespace
{
    template <class T> bool isFinite(const T& value)
    {
        for (glm::length_t i = 0; i < value.length(); ++i)
        {
            if (!std::isfinite(value[i]))
                return false;
        }
        return true;
    }
} // namespace

GtsSkeletonValidationResult validateGtsSkeletonAsset(const GtsSkeletonAsset& skeleton)
{
    GtsSkeletonValidationResult result;
    if (skeleton.nodes.empty())
        result.diagnostics.push_back({"SKELETON_EMPTY", "A skeleton requires at least one evaluation node.", "nodes"});
    if (skeleton.nodes.size() > std::numeric_limits<uint32_t>::max())
        result.diagnostics.push_back({"SKELETON_NODE_COUNT", "Node count exceeds the 32-bit index contract.", "nodes"});

    std::set<std::string_view> identifiers;
    for (size_t i = 0; i < skeleton.nodes.size(); ++i)
    {
        const auto& node     = skeleton.nodes[i];
        const auto  location = "nodes[" + std::to_string(i) + "]";
        const auto  error    = [&](const char* code, const char* message, const char* field)
        {
            result.diagnostics.push_back({code, message, location + "." + field});
        };
        if (node.id.value.empty())
            error("SKELETON_ID_EMPTY", "A node requires a nonempty stable local identifier.", "id");
        else if (!identifiers.insert(node.id.value).second)
            error("SKELETON_ID_DUPLICATE", "Stable local identifiers must be unique.", "id");

        if (node.parentIndex)
        {
            if (*node.parentIndex >= skeleton.nodes.size())
                error("SKELETON_PARENT_OUT_OF_RANGE", "Parent does not reference a skeleton node.", "parentIndex");
            else if (*node.parentIndex >= i)
                error("SKELETON_PARENT_ORDER",
                      "A parent must precede its child; self references and cycles are invalid.",
                      "parentIndex");
        }

        if (const auto* trs = std::get_if<GtsSkeletonTrs>(&node.defaultLocalTransform))
        {
            if (!isFinite(trs->translation))
                error("SKELETON_TRANSLATION_NONFINITE",
                      "Translation must be finite.",
                      "defaultLocalTransform.translation");
            if (!isFinite(trs->rotation))
                error("SKELETON_ROTATION_NONFINITE", "Rotation must be finite.", "defaultLocalTransform.rotation");
            else
            {
                double squaredLength = 0.0;
                for (glm::length_t c = 0; c < trs->rotation.length(); ++c)
                {
                    const double component = trs->rotation[c];
                    squaredLength += component * component;
                }
                // matches the existing canonical import acceptance threshold;
                // this is validity tolerance, never approximate compatibility
                if (std::abs(squaredLength - 1.0) > 0.0001)
                    error("SKELETON_ROTATION_NOT_UNIT",
                          "Rotation must have unit magnitude (squared-length tolerance 1e-4).",
                          "defaultLocalTransform.rotation");
            }
            if (!isFinite(trs->scale))
                error("SKELETON_SCALE_NONFINITE", "Scale must be finite.", "defaultLocalTransform.scale");
        }
        else if (const auto* matrix = std::get_if<glm::mat4>(&node.defaultLocalTransform))
        {
            bool finite = true;
            for (glm::length_t column = 0; column < 4; ++column)
                finite = isFinite((*matrix)[column]) && finite;
            if (!finite)
                error("SKELETON_MATRIX_NONFINITE", "Matrix elements must be finite.", "defaultLocalTransform");
            else if ((*matrix)[0][3] != 0.0f || (*matrix)[1][3] != 0.0f || (*matrix)[2][3] != 0.0f ||
                     (*matrix)[3][3] != 1.0f)
                error("SKELETON_MATRIX_NOT_AFFINE", "Matrix bottom row must be [0, 0, 0, 1].", "defaultLocalTransform");
        }
        else
            error(
                "SKELETON_TRANSFORM_INVALID", "Local transform must contain TRS or a matrix.", "defaultLocalTransform");
    }
    return result;
}
