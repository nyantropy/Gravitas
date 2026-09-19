#pragma once

#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

#include "GtsSkeletonTypes.h"
#include "GtsSkeletonValidation.h"

struct GtsSkeletonAsset;

struct GtsSkeletonCompatibilityNode
{
    GtsSkeletonNodeId         id;
    std::optional<uint32_t>   parentIndex;
    GtsSkeletonLocalTransform defaultLocalTransform;
};

// self-contained structural requirement - records have no asset identity, names,
// or references back to the source skeleton; published contents are read-only
class GtsSkeletonCompatibility
{
    public:
    GtsSkeletonCompatibility() = default;
    // explicit record construction requires validation, just like asset input
    // producers deriving from an asset should use makeGtsSkeletonCompatibility
    explicit GtsSkeletonCompatibility(std::vector<GtsSkeletonCompatibilityNode> nodes) : nodeContracts(std::move(nodes))
    {
    }

    const std::vector<GtsSkeletonCompatibilityNode>& nodes() const
    {
        return nodeContracts;
    }

    private:
    std::vector<GtsSkeletonCompatibilityNode> nodeContracts;
};

class GtsSkeletonCompatibilityResult
{
    public:
    bool succeeded() const
    {
        return value.has_value();
    }
    const GtsSkeletonCompatibility* compatibility() const
    {
        return value ? &*value : nullptr;
    }
    const std::vector<GtsSkeletonValidationError>& diagnostics() const
    {
        return validation.diagnostics;
    }

    private:
    friend GtsSkeletonCompatibilityResult makeGtsSkeletonCompatibility(const GtsSkeletonAsset& skeleton);
    GtsSkeletonCompatibilityResult() = default;
    std::optional<GtsSkeletonCompatibility> value;
    GtsSkeletonValidationResult             validation;
};

// invalid input returns diagnostics and no descriptor, while the result owns a snapshot
[[nodiscard]] GtsSkeletonCompatibilityResult makeGtsSkeletonCompatibility(const GtsSkeletonAsset& skeleton);

// invalid descriptors never match, including themselves - exact numeric equality
// retains transform alternatives and quaternion signs
[[nodiscard]] bool areGtsSkeletonCompatibilitiesEqual(const GtsSkeletonCompatibility& left,
                                                      const GtsSkeletonCompatibility& right);
[[nodiscard]] bool isGtsSkeletonCompatible(const GtsSkeletonAsset& skeleton, const GtsSkeletonCompatibility& expected);
