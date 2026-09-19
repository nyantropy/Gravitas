#include "GtsModelImportBundleValidation.h"

#include <cstddef>
#include <map>
#include <string>
#include <utility>

#include "GtsModelImportBundle.h"
#include "model/domain/animation/GtsAnimationClipValidation.h"
#include "model/domain/model/GtsModelValidation.h"
#include "model/domain/skeleton/GtsSkeletonValidation.h"

namespace
{
    void
    addError(GtsModelImportBundleValidationResult& result, const char* code, std::string message, std::string location)
    {
        result.diagnostics.push_back(
            {GtsModelDiagnosticSeverity::Error, code, std::move(message), std::move(location)});
    }
} // namespace

GtsModelImportBundleValidationResult validateGtsModelImportBundle(const GtsModelImportBundle& bundle)
{
    GtsModelImportBundleValidationResult result;
    if (!bundle.model && bundle.skeletons.empty() && bundle.animationClips.empty())
        addError(result, "MODEL_IMPORT_BUNDLE_EMPTY", "An import must produce at least one canonical asset.", "bundle");

    // object addresses locate explicitly enumerated in-memory products only
    // structural compatibility remains entirely in the skeleton/skin domains
    std::map<const GtsSkeletonAsset*, size_t> definitions;
    for (size_t i = 0; i < bundle.skeletons.size(); ++i)
    {
        const auto& skeleton = bundle.skeletons[i];
        const auto  location = "skeletons[" + std::to_string(i) + "]";
        if (!skeleton)
        {
            addError(result, "MODEL_IMPORT_SKELETON_NULL", "A listed skeleton definition must not be null.", location);
            continue;
        }
        if (skeleton.use_count() == 0)
            addError(result,
                     "MODEL_IMPORT_SKELETON_NOT_OWNED",
                     "A listed definition requires shared lifetime ownership.",
                     location);
        if (!definitions.emplace(skeleton.get(), i).second)
            addError(result,
                     "MODEL_IMPORT_SKELETON_DUPLICATE",
                     "List each produced definition object once; occurrences belong to the model.",
                     location);
        const auto validation = validateGtsSkeletonAsset(*skeleton);
        for (const auto& error : validation.diagnostics)
            result.diagnostics.push_back(
                {GtsModelDiagnosticSeverity::Error, error.code, error.message, location + "." + error.location});
    }

    for (size_t i = 0; i < bundle.animationClips.size(); ++i)
    {
        const auto& clip     = bundle.animationClips[i];
        const auto  location = "animationClips[" + std::to_string(i) + "]";
        for (const auto& error : validateGtsAnimationClip(clip).diagnostics)
            addError(result, error.code.c_str(), error.message, location + "." + error.location);
        bool matched = false;
        for (const auto& skeleton : bundle.skeletons)
            if (skeleton && isGtsSkeletonCompatible(*skeleton, clip.targetSkeletonCompatibility))
            {
                matched = true;
                break;
            }
        if (!matched)
            addError(result,
                     "MODEL_IMPORT_ANIMATION_SKELETON_MISSING",
                     "Clip compatibility has no matching skeleton definition in this bundle; external targets are not "
                     "supported.",
                     location + ".targetSkeletonCompatibility");
    }

    if (!bundle.model)
        return result;
    auto modelValidation = validateGtsModelAsset(*bundle.model);
    for (auto& error : modelValidation.diagnostics)
    {
        error.location = "model." + error.location;
        result.diagnostics.push_back(std::move(error));
    }
    for (size_t i = 0; i < bundle.model->skeletonUses.size(); ++i)
    {
        const auto& skeleton = bundle.model->skeletonUses[i].skeleton;
        if (!skeleton)
            continue; // Model validation owns null/invalid occurrence references.
        const auto location   = "model.skeletonUses[" + std::to_string(i) + "].skeleton";
        const auto definition = definitions.find(skeleton.get());
        if (definition == definitions.end())
        {
            addError(result,
                     "MODEL_IMPORT_SKELETON_NOT_ENUMERATED",
                     "Model use must share a listed definition; external skeleton linking is not implemented.",
                     location);
            continue;
        }
        const auto& listed = bundle.skeletons[definition->second];
        if (skeleton.owner_before(listed) || listed.owner_before(skeleton))
            addError(result,
                     "MODEL_IMPORT_SKELETON_OWNERSHIP",
                     "Model use and listed definition must share ownership, not merely an object address.",
                     location);
    }
    return result;
}
