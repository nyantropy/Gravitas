#include "GtsSkinBinding.h"
#include "GtsSkinBindingValidation.h"
#include "GtsSkeletonAsset.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <limits>
#include <optional>
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

    void
    requireError(const GtsSkinBindingValidationResult& result, const std::string& code, const std::string& location)
    {
        require(!result.isValid(), "Malformed binding/context must fail validation");
        for (const auto& error : result.diagnostics)
            if (error.code == code && error.location == location && !error.message.empty())
                return;
        throw std::runtime_error("Expected " + code + " at " + location);
    }

    GtsSkeletonAsset skeleton()
    {
        GtsSkeletonAsset asset;
        asset.name  = "rig";
        asset.nodes = {{{"root"}, "Root", std::nullopt, {}},
                       {{"helper"}, "Armature", 0, {}},
                       {{"hip"}, "Hip", 1, {}},
                       {{"spine"}, "Spine", 2, {}}};
        std::get<GtsSkeletonTrs>(asset.nodes[2].defaultLocalTransform).translation = {0, 2, 0};
        asset.nodes[1].defaultLocalTransform                                       = glm::mat4(1.0f);
        return asset;
    }

    GtsSkeletonCompatibility contract(const GtsSkeletonAsset& rig)
    {
        const auto derived = makeGtsSkeletonCompatibility(rig);
        require(derived.succeeded(), "Valid fixture must produce compatibility");
        return *derived.compatibility();
    }

    GtsSkinBinding binding()
    {
        GtsSkinBinding result;
        result.targetSkeletonCompatibility = contract(skeleton());
        result.joints                      = {{0, glm::mat4(1)}};
        return result;
    }

    void structureAndMappings()
    {
        auto       skin = binding();
        const auto rig  = skeleton();
        require(validateGtsSkinBinding(skin).isValid(), "Minimal explicit node zero mapping is structurally valid");
        require(validateGtsSkinBinding(skin, rig).isValid(), "Equivalent separate skeleton definition is accepted");
        require(skin.joints[0].inverseBindMatrix == glm::mat4(1),
                "Explicit entry defaults can use identity inverse bind");

        skin.joints                         = {{3, glm::mat4(1)}, {1, glm::mat4(1)}, {2, glm::mat4(1)}};
        skin.joints[0].inverseBindMatrix[3] = {1, 2, 3, 1};
        require(validateGtsSkinBinding(skin, rig).isValid(), "Subset and arbitrary slot order are allowed");
        require(skin.joints[0].skeletonNodeIndex == 3 && skin.joints[1].skeletonNodeIndex == 1,
                "Validation does not reorder local slots or mappings");

        auto dress                              = skin;
        dress.joints[0].inverseBindMatrix[3][0] = 7;
        require(areGtsSkeletonCompatibilitiesEqual(dress.targetSkeletonCompatibility, skin.targetSkeletonCompatibility),
                "Multiple bindings require the same structural contract");
        require(validateGtsSkinBinding(dress, rig).isValid() && validateGtsSkinBinding(skin, rig).isValid(),
                "Different inverse binds can consume the same skeleton");
        dress.joints.push_back({3, glm::mat4(1)});
        require(validateGtsSkinBinding(dress, rig).isValid(),
                "Duplicate node mappings with distinct inverse binds are valid");
        require(dress.joints.front().inverseBindMatrix != dress.joints.back().inverseBindMatrix,
                "Duplicate mappings retain independent slot matrices");

        skin = binding();
        skin.joints.clear();
        requireError(validateGtsSkinBinding(skin), "SKIN_JOINTS_EMPTY", "joints");
        skin                             = binding();
        skin.targetSkeletonCompatibility = {};
        requireError(validateGtsSkinBinding(skin), "SKIN_TARGET_INVALID", "targetSkeletonCompatibility.nodes");
        requireError(validateGtsSkinBinding(skin, rig), "SKIN_TARGET_INVALID", "targetSkeletonCompatibility.nodes");
        skin           = binding();
        skin.joints[0] = {};
        requireError(validateGtsSkinBinding(skin), "SKIN_NODE_UNASSIGNED", "joints[0].skeletonNodeIndex");
        requireError(validateGtsSkinBinding(skin, rig), "SKIN_NODE_UNASSIGNED", "joints[0].skeletonNodeIndex");
        skin.joints[0].skeletonNodeIndex = static_cast<uint32_t>(rig.nodes.size());
        require(validateGtsSkinBinding(skin).isValid(), "Structural validation does not inspect remap bounds");
        requireError(validateGtsSkinBinding(skin, rig), "SKIN_NODE_OUT_OF_RANGE", "joints[0].skeletonNodeIndex");
        skin.joints[0].skeletonNodeIndex = 3;
        require(validateGtsSkinBinding(skin, rig).isValid(), "Last skeleton node is in range");

        skin                             = binding();
        auto malformed                   = rig;
        malformed.nodes[1].id.value      = "root";
        auto invalidRecords              = contract(rig).nodes();
        invalidRecords[1].id.value       = "root";
        skin.targetSkeletonCompatibility = GtsSkeletonCompatibility(std::move(invalidRecords));
        requireError(validateGtsSkinBinding(skin), "SKIN_TARGET_INVALID", "targetSkeletonCompatibility.nodes[1].id");
        skin                           = binding();
        malformed.nodes[1].parentIndex = 3;
        requireError(validateGtsSkinBinding(skin, malformed), "SKIN_SKELETON_INVALID", "skeleton.nodes[1].parentIndex");
        requireError(validateGtsSkinBinding(skin, {}), "SKIN_SKELETON_INVALID", "skeleton.nodes");
    }

    void matrices()
    {
        auto       skin   = binding();
        const auto rig    = skeleton();
        auto&      matrix = skin.joints[0].inverseBindMatrix;
        matrix[0][0]      = 0;
        matrix[1][0]      = 0.25f;
        matrix[1][1]      = -2;
        matrix[3]         = {9, 8, 7, 1};
        require(validateGtsSkinBinding(skin).isValid() && validateGtsSkinBinding(skin, rig).isValid(),
                "Singular affine inverse bind with shear, reflection and translation is allowed");
        for (float value : {std::numeric_limits<float>::infinity(),
                            -std::numeric_limits<float>::infinity(),
                            std::numeric_limits<float>::quiet_NaN()})
        {
            for (glm::length_t c = 0; c < 4; ++c)
                for (glm::length_t r = 0; r < 4; ++r)
                {
                    auto bad                              = binding();
                    bad.joints[0].inverseBindMatrix[c][r] = value;
                    requireError(
                        validateGtsSkinBinding(bad), "SKIN_INVERSE_BIND_NONFINITE", "joints[0].inverseBindMatrix");
                }
        }
        for (glm::length_t c = 0; c < 4; ++c)
        {
            auto bad = binding();
            bad.joints[0].inverseBindMatrix[c][3] += 0.01f;
            requireError(validateGtsSkinBinding(bad), "SKIN_INVERSE_BIND_NOT_AFFINE", "joints[0].inverseBindMatrix");
        }
        skin.joints[0].inverseBindMatrix[3][3] = 2;
        requireError(validateGtsSkinBinding(skin, rig), "SKIN_INVERSE_BIND_NOT_AFFINE", "joints[0].inverseBindMatrix");
    }

    void compatibility()
    {
        const auto skin = binding();
        auto       rig  = skeleton();
        rig.name        = "different rig label";
        for (auto& node : rig.nodes)
            node.name = "duplicate display name";
        require(validateGtsSkinBinding(skin, rig).isValid(), "Display names never select compatibility");
        rig.nodes[3].parentIndex = 1;
        requireError(validateGtsSkinBinding(skin, rig), "SKIN_SKELETON_INCOMPATIBLE", "skeleton");
        rig                   = skeleton();
        rig.nodes[3].id.value = "different identity";
        requireError(validateGtsSkinBinding(skin, rig), "SKIN_SKELETON_INCOMPATIBLE", "skeleton");
        rig                                                                        = skeleton();
        std::get<GtsSkeletonTrs>(rig.nodes[2].defaultLocalTransform).translation.x = 1e-8f;
        requireError(validateGtsSkinBinding(skin, rig), "SKIN_SKELETON_INCOMPATIBLE", "skeleton");
        rig                                                                     = skeleton();
        std::get<GtsSkeletonTrs>(rig.nodes[2].defaultLocalTransform).rotation.x = 1e-8f;
        requireError(validateGtsSkinBinding(skin, rig), "SKIN_SKELETON_INCOMPATIBLE", "skeleton");
        rig                                                           = skeleton();
        std::get<glm::mat4>(rig.nodes[1].defaultLocalTransform)[3][0] = 1e-8f;
        requireError(validateGtsSkinBinding(skin, rig), "SKIN_SKELETON_INCOMPATIBLE", "skeleton");
        rig                                = skeleton();
        rig.nodes[0].defaultLocalTransform = glm::mat4(1);
        requireError(validateGtsSkinBinding(skin, rig), "SKIN_SKELETON_INCOMPATIBLE", "skeleton");
        rig                                                                        = skeleton();
        std::get<GtsSkeletonTrs>(rig.nodes[0].defaultLocalTransform).translation.x = -0.0f;
        require(validateGtsSkinBinding(skin, rig).isValid(),
                "Existing exact numeric compatibility preserves signed-zero policy");
        rig = skeleton();
        rig.nodes.pop_back();
        requireError(validateGtsSkinBinding(skin, rig), "SKIN_SKELETON_INCOMPATIBLE", "skeleton");
        require(skin.joints[0].skeletonNodeIndex == 0, "Full expected contract is checked even for unmapped nodes");
    }

    void immutableAndDeterministic()
    {
        GtsSkinBinding skin;
        {
            const auto source = skeleton();
            skin              = {"body", contract(source), {{3, glm::mat4(1)}, {1, glm::mat4(1)}}};
        }
        // No source skeleton is alive here; the binding is a self-contained value.
        require(validateGtsSkinBinding(skin).isValid(), "Structural validation survives source destruction");
        skin.joints[0].inverseBindMatrix[3][0] = 13;
        auto       rig                         = skeleton();
        const auto beforeSkeleton              = rig;
        const auto beforeBinding               = skin;
        require(validateGtsSkinBinding(skin, rig).isValid(),
                "Independent compatible context is accepted after source destruction");
        require(areGtsSkeletonsCompatible(rig, beforeSkeleton) &&
                    areGtsSkeletonCompatibilitiesEqual(skin.targetSkeletonCompatibility,
                                                       beforeBinding.targetSkeletonCompatibility) &&
                    skin.name == beforeBinding.name && skin.joints.size() == beforeBinding.joints.size(),
                "Validation does not replace/mutate the skeleton or binding");
        for (size_t i = 0; i < skin.joints.size(); ++i)
            require(skin.joints[i].skeletonNodeIndex == beforeBinding.joints[i].skeletonNodeIndex &&
                        skin.joints[i].inverseBindMatrix == beforeBinding.joints[i].inverseBindMatrix,
                    "Validation preserves each authored joint entry");
        require(isGtsSkeletonCompatible(beforeSkeleton, skin.targetSkeletonCompatibility),
                "Expected definition remains unchanged");

        skin.joints[0].skeletonNodeIndex       = 100;
        skin.joints[1].inverseBindMatrix[0][3] = 1;
        rig.nodes[3].id.value                  = "changed";
        const auto first                       = validateGtsSkinBinding(skin, rig);
        const auto second                      = validateGtsSkinBinding(skin, rig);
        require(first.diagnostics.size() == 3 && first.diagnostics.size() == second.diagnostics.size(),
                "Independent matrix, compatibility and remap errors are all reported");
        require(first.diagnostics[0].code == "SKIN_INVERSE_BIND_NOT_AFFINE" &&
                    first.diagnostics[1].code == "SKIN_SKELETON_INCOMPATIBLE" &&
                    first.diagnostics[2].code == "SKIN_NODE_OUT_OF_RANGE",
                "Diagnostic phase order is stable");
        for (size_t i = 0; i < first.diagnostics.size(); ++i)
            require(first.diagnostics[i].code == second.diagnostics[i].code &&
                        first.diagnostics[i].location == second.diagnostics[i].location &&
                        first.diagnostics[i].message == second.diagnostics[i].message,
                    "Diagnostic contents are deterministic");
        require(skin.joints[0].skeletonNodeIndex == 100 && skin.joints[1].inverseBindMatrix[0][3] == 1 &&
                    rig.nodes[3].id.value == "changed",
                "Failure does not repair invalid data");
    }
} // namespace

int main()
{
    try
    {
        structureAndMappings();
        matrices();
        compatibility();
        immutableAndDeterministic();
        std::puts("GtsSkinBindingTest passed");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "FAIL: %s\n", error.what());
        return 1;
    }
}
