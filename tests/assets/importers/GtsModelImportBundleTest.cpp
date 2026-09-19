#include "GtsModelImportBundleValidation.h"
#include "GtsModelImportResult.h"
#include "GtsModelValidation.h"
#include "GtsSkeletonAsset.h"

#include <cstdio>
#include <cstddef>
#include <exception>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

namespace
{
    void require(bool condition, const char* message)
    {
        if (!condition)
            throw std::runtime_error(message);
    }

    std::shared_ptr<const GtsSkeletonAsset> skeleton(const std::string& id = "root")
    {
        GtsSkeletonAsset asset;
        asset.nodes.push_back({{id}, "Root", std::nullopt, {}});
        return std::make_shared<const GtsSkeletonAsset>(std::move(asset));
    }

    GtsModelImportBundle associated()
    {
        GtsModelImportBundle bundle;
        bundle.skeletons = {skeleton()};
        bundle.model.emplace();
        bundle.model->skeletonUses = {{bundle.skeletons[0]}, {bundle.skeletons[0]}};
        GtsModelSkinBinding association;
        const auto          compatibility = makeGtsSkeletonCompatibility(*bundle.skeletons[0]);
        require(compatibility.succeeded(), "Fixture skeleton must validate");
        association.binding.targetSkeletonCompatibility = *compatibility.compatibility();
        association.binding.joints                      = {{0, glm::mat4(1)}};
        association.skeletonUseIndex                    = 0;
        bundle.model->skinBindings.push_back(std::move(association));
        return bundle;
    }

    void requireFailure(GtsModelImportBundle bundle, const std::string& code, const std::string& location)
    {
        const auto validation = validateGtsModelImportBundle(bundle);
        require(!validation.isValid(), "Malformed bundle must fail standalone validation");
        bool found = false;
        for (const auto& error : validation.diagnostics)
            found |= error.code == code && error.location == location && !error.message.empty();
        require(found, ("Missing expected diagnostic " + code + " at " + location).c_str());
        const auto result = GtsModelImportResult::success(std::move(bundle));
        require(!result.succeeded() && !result.bundle() && !result.asset(), "Failure exposes no partial products");
    }

    void productsAndOccurrences()
    {
        const auto modelOnly = GtsModelImportResult::success(GtsModelAsset{});
        require(modelOnly.succeeded() && modelOnly.bundle() && modelOnly.asset(), "Model-only convenience succeeds");
        require(modelOnly.bundle()->model.has_value() && modelOnly.bundle()->skeletons.empty() &&
                    modelOnly.bundle()->animationClips.empty(),
                "Static imports need no skeleton placeholders");
        require(modelOnly.asset() == &*modelOnly.bundle()->model, "Model accessor refers to bundle primary model");

        auto       bundle = associated();
        const auto result = GtsModelImportResult::success(bundle);
        require(result.succeeded() && result.bundle()->skeletons.size() == 1, "Associated products validate together");
        require(result.asset()->skeletonUses.size() == 2, "Definition is enumerated once for two occurrences");
        require(result.asset()->skeletonUses[0].skeleton == result.bundle()->skeletons[0] &&
                    result.asset()->skeletonUses[1].skeleton == result.bundle()->skeletons[0],
                "Bundle and occurrences share intended object");
        require(&result.asset()->skeletonUses[0] != &result.asset()->skeletonUses[1],
                "Occurrences remain distinct entries");

        bundle.skeletons.push_back(skeleton());
        require(areGtsSkeletonsCompatible(*bundle.skeletons[0], *bundle.skeletons[1]),
                "Separate definitions can be structurally compatible");
        bundle.model->skeletonUses.push_back({bundle.skeletons[1]});
        bundle.skeletons.push_back(skeleton("another-rig"));
        const auto several = GtsModelImportResult::success(bundle);
        require(several.succeeded() && several.bundle()->skeletons.size() == 3,
                "Compatible and incompatible distinct definitions are retained, including unused ones");
        require(several.bundle()->skeletons[0] != several.bundle()->skeletons[1],
                "Validation never merges by compatibility");

        bundle.model.reset();
        const auto skeletonOnly = GtsModelImportResult::success(std::move(bundle));
        require(skeletonOnly.succeeded() && skeletonOnly.bundle() && !skeletonOnly.asset(),
                "Successful bundle need not have a model");
        require(skeletonOnly.bundle()->skeletons.size() == 3, "Model-less product list is preserved");
        requireFailure({}, "MODEL_IMPORT_BUNDLE_EMPTY", "bundle");
    }

    void animationProducts()
    {
        auto                  bundle = associated();
        GtsAnimationClipAsset clip;
        clip.name                        = "Walk";
        clip.targetSkeletonCompatibility = *makeGtsSkeletonCompatibility(*bundle.skeletons[0]).compatibility();
        GtsAnimationTrack track;
        track.skeletonNodeIndex = 0;
        track.timesSeconds      = {0};
        track.values            = std::vector<glm::vec3>{{1, 2, 3}};
        clip.tracks             = {track};
        bundle.animationClips.push_back(clip);
        auto result = GtsModelImportResult::success(bundle);
        require(result.succeeded() && result.bundle()->animationClips.size() == 1 &&
                    result.asset()->skeletonUses.size() == 2,
                "Two model occurrences share one structural clip without clip duplication");
        bundle.skeletons.push_back(skeleton());
        bundle.model->skeletonUses.push_back({bundle.skeletons.back()});
        require(validateGtsModelImportBundle(bundle).isValid(),
                "Several exact compatible definitions can consume one clip");
        require(bundle.animationClips.size() == 1, "Compatibility matching never duplicates clips");
        bundle.animationClips.push_back(clip);
        bundle.animationClips.back().name = "Idle";
        require(validateGtsModelImportBundle(bundle).isValid(), "Multiple motion definitions may target one contract");
        bundle.model.reset();
        require(GtsModelImportResult::success(bundle).succeeded(), "Skeleton and clips can validate without a model");
        bundle.skeletons.clear();
        requireFailure(
            bundle, "MODEL_IMPORT_ANIMATION_SKELETON_MISSING", "animationClips[0].targetSkeletonCompatibility");
        bundle.skeletons = {skeleton("incompatible")};
        requireFailure(
            bundle, "MODEL_IMPORT_ANIMATION_SKELETON_MISSING", "animationClips[0].targetSkeletonCompatibility");
        bundle.skeletons = {skeleton()};
        bundle.animationClips[0].tracks.clear();
        requireFailure(bundle, "ANIMATION_TRACKS_EMPTY", "animationClips[0].tracks");
        bundle.animationClips[0]                             = clip;
        bundle.animationClips[0].targetSkeletonCompatibility = {};
        requireFailure(bundle, "ANIMATION_TARGET_INVALID", "animationClips[0].targetSkeletonCompatibility.nodes");
        bundle.animationClips[0] = clip;
        const GtsModelDiagnostic warning{GtsModelDiagnosticSeverity::Warning, "WARN", "Recoverable issue", {}};
        result = GtsModelImportResult::success(bundle, {warning});
        require(result.succeeded() && result.hasWarnings() && result.bundle()->animationClips.size() == 2,
                "Warnings preserve every clip");
        const auto failed =
            GtsModelImportResult::success(bundle, {{GtsModelDiagnosticSeverity::Error, "ERR", "Failure", {}}});
        require(!failed.bundle() && !failed.asset(), "Errors expose no partial clips or graph");
    }

    void invalidGraphs()
    {
        auto bundle = associated();
        bundle.skeletons.push_back(nullptr);
        requireFailure(bundle, "MODEL_IMPORT_SKELETON_NULL", "skeletons[1]");
        bundle              = associated();
        bundle.skeletons[0] = std::make_shared<const GtsSkeletonAsset>();
        require(!validateGtsModelImportBundle(bundle).isValid(),
                "Invalid listed definition is rejected even when model references a valid different one");
        const auto invalid = GtsModelImportResult::success(bundle);
        require(!invalid.succeeded() && !invalid.bundle(), "Invalid skeleton cannot escape as successful product");

        bundle                  = associated();
        bundle.model->rootNodes = {9};
        requireFailure(bundle, "MODEL_ROOT_OUT_OF_RANGE", "model.rootNodes");
        bundle = associated();
        bundle.model->skeletonUses[0].skeleton.reset();
        requireFailure(bundle, "MODEL_SKELETON_REQUIRED", "model.skeletonUses[0].skeleton");
        bundle                                         = associated();
        bundle.model->skinBindings[0].skeletonUseIndex = 99;
        requireFailure(bundle, "MODEL_SKELETON_USE_OUT_OF_RANGE", "model.skinBindings[0].skeletonUseIndex");
        bundle                                                            = associated();
        bundle.model->skinBindings[0].binding.joints[0].skeletonNodeIndex = 8;
        require(!validateGtsModelImportBundle(bundle).isValid(), "Skin contextual validation is composed");

        bundle = associated();
        bundle.skeletons.push_back(bundle.skeletons[0]);
        requireFailure(bundle, "MODEL_IMPORT_SKELETON_DUPLICATE", "skeletons[1]");
        bundle                                 = associated();
        bundle.model->skeletonUses[0].skeleton = skeleton();
        require(validateGtsModelAsset(*bundle.model).isValid(),
                "Standalone model is valid with compatible independent definition");
        requireFailure(bundle, "MODEL_IMPORT_SKELETON_NOT_ENUMERATED", "model.skeletonUses[0].skeleton");
        bundle               = associated();
        const auto modelOnly = GtsModelImportResult::success(*bundle.model);
        require(!modelOnly.succeeded(), "Model convenience does not silently collect hidden skeleton definitions");

        bundle = associated();
        // A second control block with a no-op deleter is safe for this fixture,
        // but does not retain the definition after the real owner is released.
        bundle.model->skeletonUses[0].skeleton =
            std::shared_ptr<const GtsSkeletonAsset>(bundle.skeletons[0].get(), [](const auto*) {});
        requireFailure(bundle, "MODEL_IMPORT_SKELETON_OWNERSHIP", "model.skeletonUses[0].skeleton");
        const auto owner = skeleton();
        bundle           = {};
        bundle.skeletons.emplace_back(std::shared_ptr<const GtsSkeletonAsset>{}, owner.get());
        requireFailure(bundle, "MODEL_IMPORT_SKELETON_NOT_OWNED", "skeletons[0]");
    }

    void diagnostics()
    {
        const GtsModelDiagnostic warning{
            GtsModelDiagnosticSeverity::Warning, "SOURCE_WARNING", "Recoverable source issue.", "source"};
        const GtsModelDiagnostic error{
            GtsModelDiagnosticSeverity::Error, "SOURCE_ERROR", "Required feature unsupported.", "source"};
        const auto warned = GtsModelImportResult::success(associated(), {warning});
        require(warned.succeeded() && warned.hasWarnings() && warned.bundle(), "Warnings retain the whole bundle");
        const auto failed = GtsModelImportResult::success(associated(), {warning, error});
        require(!failed.succeeded() && !failed.bundle() && !failed.asset() && failed.hasWarnings(),
                "Source errors discard all products while retaining diagnostics");
        const auto explicitFailure = GtsModelImportResult::failure({warning});
        require(!explicitFailure.bundle() && explicitFailure.diagnostics().size() == 2,
                "Failure adds an error when necessary");
        auto bundle = associated();
        bundle.skeletons.push_back(nullptr);
        bundle.model->rootNodes.push_back(99);
        const auto a = validateGtsModelImportBundle(bundle);
        const auto b = validateGtsModelImportBundle(bundle);
        require(a.diagnostics.size() == b.diagnostics.size() && !a.isValid(), "Validation results are deterministic");
        for (size_t i = 0; i < a.diagnostics.size(); ++i)
            require(a.diagnostics[i].code == b.diagnostics[i].code &&
                        a.diagnostics[i].location == b.diagnostics[i].location &&
                        a.diagnostics[i].message == b.diagnostics[i].message,
                    "Diagnostic ordering and contents are stable");
        require(bundle.skeletons.size() == 2 && !bundle.skeletons[1] && bundle.model->rootNodes[0] == 99,
                "Validation does not repair/mutate products");
    }

    void lifetime()
    {
        std::weak_ptr<const GtsSkeletonAsset>   weak;
        std::optional<GtsModelAsset>            retainedModel;
        std::shared_ptr<const GtsSkeletonAsset> retainedDefinition;
        {
            auto bundle       = associated();
            weak              = bundle.skeletons[0];
            const auto result = GtsModelImportResult::success(std::move(bundle));
            require(result.succeeded() && !weak.expired(), "Successful result retains produced definition");
            retainedModel      = *result.asset();
            retainedDefinition = result.bundle()->skeletons[0];
            require(retainedDefinition == retainedModel->skeletonUses[0].skeleton,
                    "Extraction does not copy skeleton definition");
        }
        require(!weak.expired(), "Extracted graph survives result destruction");
        retainedDefinition.reset();
        require(!weak.expired(), "Model occurrences independently retain definition");
        retainedModel.reset();
        require(weak.expired(), "Definition dies after the last graph owner");
        {
            auto bundle = associated();
            weak        = bundle.skeletons[0];
            bundle.model.reset();
            const auto result = GtsModelImportResult::success(std::move(bundle));
            require(result.succeeded() && !weak.expired(), "Bundle list owns definition without any model occurrence");
        }
        require(weak.expired(), "Skeleton-only result releases its ownership");
        {
            auto bundle       = associated();
            weak              = bundle.skeletons[0];
            const auto result = GtsModelImportResult::success(
                std::move(bundle), {{GtsModelDiagnosticSeverity::Error, "FAIL", "Failure", {}}});
            require(!result.succeeded() && weak.expired(), "Failed result retains diagnostics, not hidden assets");
        }
    }
} // namespace

int main()
{
    try
    {
        productsAndOccurrences();
        animationProducts();
        invalidGraphs();
        diagnostics();
        lifetime();
        std::puts("GtsModelImportBundleTest passed");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "FAIL: %s\n", error.what());
        return 1;
    }
}
