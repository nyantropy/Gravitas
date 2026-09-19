#include "model/domain/skeleton/GtsSkeletonAsset.h"
#include "model/domain/skeleton/GtsSkeletonCompatibility.h"
#include "model/domain/skeleton/GtsSkeletonValidation.h"

#include <cstddef>
#include <cstdio>
#include <exception>
#include <limits>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace
{
    void require(bool condition, const char* message)
    {
        if (!condition)
            throw std::runtime_error(message);
    }

    GtsSkeletonAsset skeleton()
    {
        return {"display name",
                {{{"root"}, "Root", std::nullopt, {}},
                 {{"helper"}, "Helper", 0, glm::mat4(1)},
                 {{"hip"}, "Hip", 1, {}},
                 {{"spine"}, "Spine", 1, {}}}};
    }

    GtsSkeletonCompatibility derive(const GtsSkeletonAsset& asset)
    {
        const auto result = makeGtsSkeletonCompatibility(asset);
        require(result.succeeded() && result.compatibility() && result.diagnostics().empty(),
                "Valid asset produces a descriptor without errors");
        return *result.compatibility();
    }

    void comparison()
    {
        const auto asset    = skeleton();
        const auto expected = derive(asset);
        require(expected.nodes().size() == asset.nodes.size(), "Every node participates, including helpers");
        require(validateGtsSkeletonCompatibility(expected).isValid(), "Derived descriptor is valid");
        const auto check = [&](const GtsSkeletonAsset& other, bool compatible)
        {
            const auto actual = derive(other);
            require(areGtsSkeletonCompatibilitiesEqual(expected, actual) == compatible,
                    "Descriptor equality follows exact structure");
            require(areGtsSkeletonCompatibilitiesEqual(actual, expected) == compatible, "Equality is symmetric");
            require(isGtsSkeletonCompatible(other, expected) == compatible &&
                        areGtsSkeletonsCompatible(asset, other) == compatible,
                    "All comparison APIs share one definition");
        };
        check(asset, true);
        auto changed = asset;
        changed.name = "new asset label";
        for (auto& node : changed.nodes)
            node.name = "duplicate node label";
        check(changed, true);
        changed.nodes[3].id.value = "new identity";
        check(changed, false);
        changed = asset;
        std::swap(changed.nodes[2], changed.nodes[3]);
        check(changed, false);
        changed                      = asset;
        changed.nodes[3].parentIndex = 2;
        check(changed, false);
        changed = asset;
        changed.nodes.pop_back();
        check(changed, false);
        changed                                                                        = asset;
        std::get<GtsSkeletonTrs>(changed.nodes[2].defaultLocalTransform).translation.x = 1e-8f;
        check(changed, false);
        changed                                                                  = asset;
        std::get<GtsSkeletonTrs>(changed.nodes[2].defaultLocalTransform).scale.z = 2;
        check(changed, false);
        changed                                                                     = asset;
        std::get<GtsSkeletonTrs>(changed.nodes[2].defaultLocalTransform).rotation.x = 1e-8f;
        check(changed, false);
        changed                                                                   = asset;
        std::get<GtsSkeletonTrs>(changed.nodes[2].defaultLocalTransform).rotation = glm::quat(-1, 0, 0, 0);
        check(changed, false);
        changed                                = asset;
        changed.nodes[0].defaultLocalTransform = glm::mat4(1);
        check(changed, false);
        changed                                                           = asset;
        std::get<glm::mat4>(changed.nodes[1].defaultLocalTransform)[3][0] = 1e-8f;
        check(changed, false);
        changed                                                                        = asset;
        std::get<glm::mat4>(changed.nodes[1].defaultLocalTransform)[3][0]              = -0.0f;
        std::get<GtsSkeletonTrs>(changed.nodes[2].defaultLocalTransform).translation.x = -0.0f;
        check(changed, true);
    }

    void validation()
    {
        const auto valid = derive(skeleton());
        const auto check = [&](std::vector<GtsSkeletonCompatibilityNode> nodes, const std::string& code)
        {
            const GtsSkeletonCompatibility invalid(std::move(nodes));
            const auto                     result = validateGtsSkeletonCompatibility(invalid);
            require(!result.isValid(), "Malformed descriptor must fail validation");
            bool found = false;
            for (const auto& error : result.diagnostics)
                found |= error.code == code && !error.location.empty() && !error.message.empty();
            require(found, "Descriptor validation preserves skeleton diagnostics");
            require(!areGtsSkeletonCompatibilitiesEqual(invalid, invalid) &&
                        !areGtsSkeletonCompatibilitiesEqual(valid, invalid) &&
                        !isGtsSkeletonCompatible(skeleton(), invalid),
                    "Invalid descriptors never claim compatibility");

            // The same records must fail the same validation in an asset.
            GtsSkeletonAsset asset;
            for (const auto& node : invalid.nodes())
                asset.nodes.push_back({node.id, "ignored", node.parentIndex, node.defaultLocalTransform});
            const auto derived = makeGtsSkeletonCompatibility(asset);
            require(!derived.succeeded() && !derived.compatibility(), "Invalid asset exposes no descriptor");
            require(result.diagnostics.size() == derived.diagnostics().size(),
                    "Shared validation has identical errors");
            for (size_t i = 0; i < result.diagnostics.size(); ++i)
                require(result.diagnostics[i].code == derived.diagnostics()[i].code &&
                            result.diagnostics[i].message == derived.diagnostics()[i].message &&
                            result.diagnostics[i].location == derived.diagnostics()[i].location,
                        "Asset and descriptor checks cannot drift");
        };
        check({}, "SKELETON_EMPTY");
        auto nodes = valid.nodes();
        nodes[0].id.value.clear();
        check(nodes, "SKELETON_ID_EMPTY");
        nodes       = valid.nodes();
        nodes[1].id = nodes[0].id;
        check(nodes, "SKELETON_ID_DUPLICATE");
        nodes                = valid.nodes();
        nodes[1].parentIndex = 99;
        check(nodes, "SKELETON_PARENT_OUT_OF_RANGE");
        nodes[1].parentIndex = 3;
        check(nodes, "SKELETON_PARENT_ORDER");
        nodes[1].parentIndex = 1;
        check(nodes, "SKELETON_PARENT_ORDER");
        nodes                = valid.nodes();
        nodes[0].parentIndex = 3;
        check(nodes, "SKELETON_PARENT_ORDER");
        for (float value : {std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()})
        {
            nodes                                                                  = valid.nodes();
            std::get<GtsSkeletonTrs>(nodes[0].defaultLocalTransform).translation.x = value;
            check(nodes, "SKELETON_TRANSLATION_NONFINITE");
            nodes                                                               = valid.nodes();
            std::get<GtsSkeletonTrs>(nodes[0].defaultLocalTransform).rotation.w = value;
            check(nodes, "SKELETON_ROTATION_NONFINITE");
            nodes                                                            = valid.nodes();
            std::get<GtsSkeletonTrs>(nodes[0].defaultLocalTransform).scale.y = value;
            check(nodes, "SKELETON_SCALE_NONFINITE");
            nodes                                                     = valid.nodes();
            std::get<glm::mat4>(nodes[1].defaultLocalTransform)[2][1] = value;
            check(nodes, "SKELETON_MATRIX_NONFINITE");
        }
        for (float magnitude : {0.0f, 2.0f})
        {
            nodes                                                               = valid.nodes();
            std::get<GtsSkeletonTrs>(nodes[0].defaultLocalTransform).rotation.w = magnitude;
            check(nodes, "SKELETON_ROTATION_NOT_UNIT");
        }
        nodes                                                     = valid.nodes();
        std::get<glm::mat4>(nodes[1].defaultLocalTransform)[0][3] = 0.001f;
        check(nodes, "SKELETON_MATRIX_NOT_AFFINE");
        nodes                                                            = valid.nodes();
        std::get<glm::mat4>(nodes[1].defaultLocalTransform)[0][0]        = 0;
        std::get<GtsSkeletonTrs>(nodes[0].defaultLocalTransform).scale.x = 0;
        require(validateGtsSkeletonCompatibility(GtsSkeletonCompatibility(nodes)).isValid(),
                "Singular affine and zero-scale semantics remain unchanged");
    }

    void valueOwnership()
    {
        static_assert(std::is_same_v<decltype(std::declval<GtsSkeletonCompatibility&>().nodes()),
                                     const std::vector<GtsSkeletonCompatibilityNode>&>);
        GtsSkeletonCompatibility saved;
        {
            auto source              = skeleton();
            saved                    = derive(source);
            source.nodes[0].id.value = "changed after derivation";
            require(saved.nodes()[0].id.value == "root", "Descriptor owns its structural snapshot");
            require(!isGtsSkeletonCompatible(source, saved), "Source edits do not change expectation");
        }
        require(validateGtsSkeletonCompatibility(saved).isValid() && isGtsSkeletonCompatible(skeleton(), saved),
                "Descriptor survives source lifetime without pointers");
        const auto before = saved;
        const auto first  = validateGtsSkeletonCompatibility(saved);
        const auto second = validateGtsSkeletonCompatibility(saved);
        require(first.isValid() && second.isValid() && areGtsSkeletonCompatibilitiesEqual(before, saved),
                "Validation and comparison leave descriptor contents unchanged");
    }
} // namespace

int main()
{
    try
    {
        comparison();
        validation();
        valueOwnership();
        std::puts("GtsSkeletonCompatibilityTest passed");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "FAIL: %s\n", error.what());
        return 1;
    }
}
