#include "GtsSkeletonCompatibility.h"

#include <cstddef>
#include <utility>
#include <variant>
#include <vector>

#include "GtsSkeletonAsset.h"

#include "GtsSkeletonValidation.h"

namespace
{
    // bundled GLM equality differs between vectors and quaternions, as the domain
    // requires exact numeric component equality for every transform form
    template <class T> bool equalComponents(const T& left, const T& right)
    {
        for (glm::length_t i = 0; i < left.length(); ++i)
        {
            if (left[i] != right[i])
                return false;
        }
        return true;
    }
} // namespace

bool areGtsSkeletonCompatibilitiesEqual(const GtsSkeletonCompatibility& left, const GtsSkeletonCompatibility& right)
{
    if (!validateGtsSkeletonCompatibility(left).isValid() || !validateGtsSkeletonCompatibility(right).isValid())
        return false;
    if (left.nodes().size() != right.nodes().size())
        return false;

    for (size_t i = 0; i < left.nodes().size(); ++i)
    {
        const auto& a = left.nodes()[i];
        const auto& b = right.nodes()[i];
        if (a.id != b.id || a.parentIndex != b.parentIndex ||
            a.defaultLocalTransform.index() != b.defaultLocalTransform.index())
            return false;
        if (const auto* trs = std::get_if<GtsSkeletonTrs>(&a.defaultLocalTransform))
        {
            const auto& other = std::get<GtsSkeletonTrs>(b.defaultLocalTransform);
            if (!equalComponents(trs->translation, other.translation) ||
                !equalComponents(trs->rotation, other.rotation) || !equalComponents(trs->scale, other.scale))
                return false;
        }
        else
        {
            const auto& matrix = std::get<glm::mat4>(a.defaultLocalTransform);
            const auto& other  = std::get<glm::mat4>(b.defaultLocalTransform);
            for (glm::length_t column = 0; column < 4; ++column)
            {
                if (!equalComponents(matrix[column], other[column]))
                    return false;
            }
        }
    }
    return true;
}

GtsSkeletonCompatibilityResult makeGtsSkeletonCompatibility(const GtsSkeletonAsset& skeleton)
{
    GtsSkeletonCompatibilityResult result;
    result.validation = validateGtsSkeletonAsset(skeleton);
    if (!result.validation.isValid())
        return result;
    std::vector<GtsSkeletonCompatibilityNode> nodes;
    nodes.reserve(skeleton.nodes.size());
    for (const auto& node : skeleton.nodes)
        nodes.push_back({node.id, node.parentIndex, node.defaultLocalTransform});
    result.value.emplace(std::move(nodes));
    return result;
}

bool isGtsSkeletonCompatible(const GtsSkeletonAsset& skeleton, const GtsSkeletonCompatibility& expected)
{
    const auto derived = makeGtsSkeletonCompatibility(skeleton);
    return derived.succeeded() && areGtsSkeletonCompatibilitiesEqual(*derived.compatibility(), expected);
}
