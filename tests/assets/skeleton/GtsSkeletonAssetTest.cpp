#include "GtsSkeletonAsset.h"
#include "GtsSkeletonValidation.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>

namespace
{
    void require(bool condition, const char* message)
    {
        if (!condition)
            throw std::runtime_error(message);
    }

    void requireError(const GtsSkeletonAsset& asset, const std::string& code, const std::string& location)
    {
        const auto result = validateGtsSkeletonAsset(asset);
        require(!result.isValid(), "Malformed skeleton must fail validation");
        for (const auto& error : result.diagnostics)
        {
            if (error.code == code && error.location == location && !error.message.empty())
                return;
        }
        throw std::runtime_error("Expected diagnostic: " + code + " at " + location);
    }

    GtsSkeletonAsset hierarchy()
    {
        GtsSkeletonAsset asset;
        asset.name  = "character";
        asset.nodes = {{{"rig-root"}, "Root", std::nullopt, {}},
                       {{"rig-helper"}, "Armature", 0, {}},
                       {{"hip"}, "Hip", 1, {}},
                       {{"spine"}, "Spine", 2, {}}};
        std::get<GtsSkeletonTrs>(asset.nodes[1].defaultLocalTransform).translation = {0, 2, 0};
        return asset;
    }

    void hierarchyValidation()
    {
        GtsSkeletonAsset minimal;
        minimal.nodes.push_back({{"root"}, "", std::nullopt, {}});
        require(validateGtsSkeletonAsset(minimal).isValid(), "One root with identity defaults is valid");
        requireError({}, "SKELETON_EMPTY", "nodes");
        minimal.nodes[0].id = {};
        requireError(minimal, "SKELETON_ID_EMPTY", "nodes[0].id");

        const auto asset = hierarchy();
        require(validateGtsSkeletonAsset(asset).isValid(), "Parent-first hierarchy retains non-deforming helpers");
        auto changed = asset;
        changed.nodes.push_back({{"other-root"}, "", std::nullopt, {}});
        changed.nodes.push_back({{"other-child"}, "", 4, {}});
        require(validateGtsSkeletonAsset(changed).isValid(), "Multiple roots and their children are valid");
        for (auto& node : changed.nodes)
            node.name = "duplicate display name";
        require(validateGtsSkeletonAsset(changed).isValid(), "Display names need not be unique");
        changed.nodes[1].id = changed.nodes[0].id;
        requireError(changed, "SKELETON_ID_DUPLICATE", "nodes[1].id");

        changed                      = asset;
        changed.nodes[1].parentIndex = static_cast<uint32_t>(asset.nodes.size());
        requireError(changed, "SKELETON_PARENT_OUT_OF_RANGE", "nodes[1].parentIndex");
        changed.nodes[1].parentIndex = std::numeric_limits<uint32_t>::max();
        requireError(changed, "SKELETON_PARENT_OUT_OF_RANGE", "nodes[1].parentIndex");
        changed.nodes[1].parentIndex = 3;
        requireError(changed, "SKELETON_PARENT_ORDER", "nodes[1].parentIndex");
        changed.nodes[1].parentIndex = 1;
        requireError(changed, "SKELETON_PARENT_ORDER", "nodes[1].parentIndex");
        changed                      = asset;
        changed.nodes[0].parentIndex = 3;
        requireError(changed, "SKELETON_PARENT_ORDER", "nodes[0].parentIndex");

        GtsSkeletonAsset deep;
        for (uint32_t i = 0; i < 10000; ++i)
            deep.nodes.push_back(
                {{"node-" + std::to_string(i)}, "", i ? std::optional<uint32_t>(i - 1) : std::nullopt, {}});
        require(validateGtsSkeletonAsset(deep).isValid(), "Deep hierarchy validation needs no recursive traversal");
    }

    void transforms()
    {
        auto        asset    = hierarchy();
        const auto& defaults = std::get<GtsSkeletonTrs>(asset.nodes[0].defaultLocalTransform);
        require(defaults.translation == glm::vec3(0) && defaults.rotation == glm::quat(1, 0, 0, 0) &&
                    defaults.scale == glm::vec3(1),
                "Default local transform is identity quaternion TRS");
        auto& trs       = std::get<GtsSkeletonTrs>(asset.nodes[2].defaultLocalTransform);
        trs.translation = {1, 2, 3};
        trs.rotation    = glm::angleAxis(0.7f, glm::vec3(0, 1, 0));
        trs.scale       = {0, -2, 3};
        glm::mat4 matrix(1);
        matrix[0][0]                         = 0;
        matrix[1][0]                         = 0.25f;
        matrix[3]                            = {4, 5, 6, 1};
        asset.nodes[1].defaultLocalTransform = matrix;
        require(validateGtsSkeletonAsset(asset).isValid(), "Finite TRS and singular/sheared affine helpers are valid");
        const auto before = asset;
        require(areGtsSkeletonsCompatible(asset, before), "Mixed transforms compare without conversion");
        require(std::get<glm::mat4>(asset.nodes[1].defaultLocalTransform) == matrix,
                "Validation preserves exact matrix data");
        require(std::get<GtsSkeletonTrs>(asset.nodes[2].defaultLocalTransform).rotation ==
                    std::get<GtsSkeletonTrs>(before.nodes[2].defaultLocalTransform).rotation,
                "Validation does not normalize authored rotations");
        asset.nodes[0].defaultLocalTransform = glm::mat4(1);
        require(validateGtsSkeletonAsset(asset).isValid(), "Explicit identity matrix is valid");

        for (float value : {std::numeric_limits<float>::infinity(),
                            -std::numeric_limits<float>::infinity(),
                            std::numeric_limits<float>::quiet_NaN()})
        {
            auto invalid                                                                   = hierarchy();
            std::get<GtsSkeletonTrs>(invalid.nodes[0].defaultLocalTransform).translation.y = value;
            requireError(invalid, "SKELETON_TRANSLATION_NONFINITE", "nodes[0].defaultLocalTransform.translation");
            invalid                                                                     = hierarchy();
            std::get<GtsSkeletonTrs>(invalid.nodes[0].defaultLocalTransform).rotation.z = value;
            requireError(invalid, "SKELETON_ROTATION_NONFINITE", "nodes[0].defaultLocalTransform.rotation");
            invalid                                                                  = hierarchy();
            std::get<GtsSkeletonTrs>(invalid.nodes[0].defaultLocalTransform).scale.x = value;
            requireError(invalid, "SKELETON_SCALE_NONFINITE", "nodes[0].defaultLocalTransform.scale");
            for (glm::length_t c = 0; c < 4; ++c)
            {
                invalid                                = hierarchy();
                auto badMatrix                         = glm::mat4(1);
                badMatrix[c][2]                        = value;
                invalid.nodes[0].defaultLocalTransform = badMatrix;
                requireError(invalid, "SKELETON_MATRIX_NONFINITE", "nodes[0].defaultLocalTransform");
            }
        }
        for (float magnitude : {0.0f, 0.1f, 2.0f, std::numeric_limits<float>::max()})
        {
            auto invalid                                                              = hierarchy();
            std::get<GtsSkeletonTrs>(invalid.nodes[0].defaultLocalTransform).rotation = glm::quat(magnitude, 0, 0, 0);
            requireError(invalid, "SKELETON_ROTATION_NOT_UNIT", "nodes[0].defaultLocalTransform.rotation");
        }
        for (glm::length_t c = 0; c < 4; ++c)
        {
            auto invalid   = hierarchy();
            auto badMatrix = glm::mat4(1);
            badMatrix[c][3] += 0.001f;
            invalid.nodes[0].defaultLocalTransform = badMatrix;
            requireError(invalid, "SKELETON_MATRIX_NOT_AFFINE", "nodes[0].defaultLocalTransform");
        }
    }

    void compatibility()
    {
        const auto asset = hierarchy();
        require(areGtsSkeletonsCompatible(asset, asset), "Valid skeleton is compatible with itself");
        auto changed = asset;
        changed.name = "renamed asset";
        for (auto& node : changed.nodes)
            node.name = "renamed node";
        require(areGtsSkeletonsCompatible(asset, changed), "Display names do not participate in compatibility");
        changed.nodes[3].parentIndex = 1;
        require(validateGtsSkeletonAsset(changed).isValid(), "Changed hierarchy is still valid");
        require(!areGtsSkeletonsCompatible(asset, changed), "Changed parent breaks compatibility");
        changed                   = asset;
        changed.nodes[3].id.value = "different-id";
        require(!areGtsSkeletonsCompatible(asset, changed), "Changed stable ID breaks compatibility");
        changed = asset;
        changed.nodes.pop_back();
        require(!areGtsSkeletonsCompatible(asset, changed), "Changed node count breaks compatibility");
        changed                                                                        = asset;
        std::get<GtsSkeletonTrs>(changed.nodes[2].defaultLocalTransform).translation.x = std::nextafter(0.0f, 1.0f);
        require(!areGtsSkeletonsCompatible(asset, changed), "Even the smallest finite translation change is unequal");
        changed                                                                  = asset;
        std::get<GtsSkeletonTrs>(changed.nodes[2].defaultLocalTransform).scale.z = 2;
        require(!areGtsSkeletonsCompatible(asset, changed), "Changed scale breaks compatibility");
        changed = asset;
        std::get<GtsSkeletonTrs>(changed.nodes[2].defaultLocalTransform).rotation =
            glm::angleAxis(0.5f, glm::vec3(1, 0, 0));
        require(!areGtsSkeletonsCompatible(asset, changed), "Changed quaternion breaks compatibility");
        changed                                                                     = asset;
        std::get<GtsSkeletonTrs>(changed.nodes[2].defaultLocalTransform).rotation.x = 1e-8f;
        require(validateGtsSkeletonAsset(changed).isValid(), "Tiny rotation remains valid");
        require(!areGtsSkeletonsCompatible(asset, changed), "Quaternion comparison must not use GLM epsilon equality");
        changed                                                                   = asset;
        std::get<GtsSkeletonTrs>(changed.nodes[2].defaultLocalTransform).rotation = glm::quat(-1, 0, 0, 0);
        require(!areGtsSkeletonsCompatible(asset, changed), "Quaternion sign equivalence is not stored-value equality");
        changed                                = asset;
        changed.nodes[0].defaultLocalTransform = glm::mat4(1);
        require(!areGtsSkeletonsCompatible(asset, changed), "Equivalent matrix/TRS forms are intentionally distinct");
        const auto matrixAsset                                            = changed;
        std::get<glm::mat4>(changed.nodes[0].defaultLocalTransform)[3][0] = 2;
        require(!areGtsSkeletonsCompatible(matrixAsset, changed), "Changed matrix breaks compatibility");
        changed                                                           = matrixAsset;
        std::get<glm::mat4>(changed.nodes[0].defaultLocalTransform)[3][0] = -0.0f;
        require(areGtsSkeletonsCompatible(matrixAsset, changed), "Matrix components use numeric signed-zero equality");
        std::get<glm::mat4>(changed.nodes[0].defaultLocalTransform)[3][0] = 1e-8f;
        require(!areGtsSkeletonsCompatible(matrixAsset, changed), "Small matrix differences remain incompatible");
        changed                                                                        = asset;
        std::get<GtsSkeletonTrs>(changed.nodes[0].defaultLocalTransform).translation.x = -0.0f;
        require(areGtsSkeletonsCompatible(asset, changed), "Numeric equality treats signed zero equally");

        auto forest                 = asset;
        forest.nodes[2].parentIndex = 0;
        forest.nodes[3].parentIndex = 0;
        changed                     = forest;
        std::swap(changed.nodes[2], changed.nodes[3]);
        require(validateGtsSkeletonAsset(changed).isValid(), "Sibling reordering can preserve a valid forest");
        require(!areGtsSkeletonsCompatible(forest, changed), "Canonical indexed ordering is part of compatibility");

        changed        = asset;
        auto& rotation = std::get<GtsSkeletonTrs>(changed.nodes[0].defaultLocalTransform).rotation;
        rotation.w     = 1.00001f;
        require(validateGtsSkeletonAsset(changed).isValid(), "Unit quaternion validation allows float roundoff");
        require(rotation.w == 1.00001f && !areGtsSkeletonsCompatible(asset, changed),
                "Validity tolerance neither repairs data nor becomes fuzzy compatibility");
        changed.nodes[0].id.value.clear();
        require(!areGtsSkeletonsCompatible(changed, changed) && !areGtsSkeletonsCompatible(asset, changed) &&
                    !areGtsSkeletonsCompatible(changed, asset),
                "Invalid assets never claim compatibility");
        require(!areGtsSkeletonsCompatible({}, {}), "Empty invalid definitions are not compatible");

        const auto first  = validateGtsSkeletonAsset(changed);
        const auto second = validateGtsSkeletonAsset(changed);
        require(first.diagnostics.size() == second.diagnostics.size(), "Diagnostic count is deterministic");
        for (size_t i = 0; i < first.diagnostics.size(); ++i)
        {
            require(first.diagnostics[i].code == second.diagnostics[i].code &&
                        first.diagnostics[i].location == second.diagnostics[i].location &&
                        first.diagnostics[i].message == second.diagnostics[i].message,
                    "Diagnostic order and contents are deterministic");
        }
        require(areGtsSkeletonsCompatible(asset, hierarchy()) && areGtsSkeletonsCompatible(hierarchy(), asset),
                "Comparison is deterministic and symmetric");
    }
} // namespace

int main()
{
    try
    {
        hierarchyValidation();
        transforms();
        compatibility();
        std::puts("GtsSkeletonAssetTest passed");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "FAIL: %s\n", error.what());
        return 1;
    }
}
