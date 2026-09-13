#include "GtsModelImportBundleValidation.h"

#include <cstddef>
#include <map>
#include <string>
#include <utility>

#include "GtsModelImportBundle.h"
#include "assets/model/GtsModelValidation.h"
#include "assets/skeleton/GtsSkeletonValidation.h"

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
    if (!bundle.model && bundle.skeletons.empty())
        addError(
            result, "MODEL_IMPORT_BUNDLE_EMPTY", "An import must produce a model or skeleton definition.", "bundle");

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
