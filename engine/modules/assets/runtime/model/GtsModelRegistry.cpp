#include "GtsModelRegistry.h"

#include <algorithm>
#include <cctype>
#include <string>
#include <utility>

#include "GtsModelResource.h"
#include "GtsModelSourceLoading.h"
#include "GtsModelCookedLoading.h"
#include "assets/importer/GtsModelImportBundleValidation.h"
#include "assets/model/GtsModelImportResult.h"
#include "assets/runtime/RuntimeAssetPolicy.h"

namespace
{
    std::string extensionOf(const std::filesystem::path& path)
    {
        auto extension = path.extension().string();
        std::transform(extension.begin(),
                       extension.end(),
                       extension.begin(),
                       [](unsigned char c)
                       {
                           return static_cast<char>(std::tolower(c));
                       });
        return extension;
    }
} // namespace

GtsModelRequestResult::GtsModelRequestResult(GtsModelHandle resource, std::vector<GtsModelDiagnostic> diagnostics)
    : resource(std::move(resource)), messages(std::move(diagnostics))
{
}

GtsModelRequestResult GtsModelRegistry::requestModel(const std::filesystem::path& path)
{
    return requestModel(GtsModelRequest{path, {}});
}

GtsModelRequestResult GtsModelRegistry::requestModel(const GtsModelRequest& request)
{
    namespace fs = std::filesystem;
    using namespace gts::assets;
    const auto&                     path = request.path;
    std::vector<GtsModelDiagnostic> diagnostics;
    auto                            failure = [&](const char* code, const std::string& message)
    {
        diagnostics.push_back({GtsModelDiagnosticSeverity::Error, code, message, path.string()});
        return GtsModelRequestResult({}, std::move(diagnostics));
    };
    if (path.empty())
        return failure("model.request.path", "Model request path is empty");
    std::error_code error;
    const auto      absolute = fs::absolute(path, error);
    if (error)
        return failure("model.request.path", error.message());
    const auto identity = fs::weakly_canonical(absolute, error);
    if (error)
        return failure("model.request.path", error.message());
    const auto extension      = extensionOf(identity);
    const bool explicitCooked = extension == ".gmesh" || extension == ".gmodel";
    if (!explicitCooked && !isRuntimeSourceMeshAssetPath(identity))
    {
        return failure("model.request.unsupported",
                       "Unsupported model input; expected .obj, .gltf, .glb, .gmesh or .gmodel");
    }

    // Evaluate policy on every request, before any cached source can be used.
    const bool            sourceAllowed = runtimeSourceAssetFallbackAllowed();
    std::vector<fs::path> candidates;
    if (explicitCooked)
        candidates.push_back(identity);
    else
    {
        candidates.push_back(expectedCookedModelAssetPath(identity));
        candidates.push_back(expectedCookedMeshAssetPath(identity));
    }

    auto publish = [&](const fs::path&                 representation,
                       GtsModelHandle                  resource,
                       std::vector<GtsModelDiagnostic> loadDiagnostics,
                       bool                            cooked)
    {
        entries.emplace(EntryKey{identity, representation, cooked}, Entry{resource, loadDiagnostics});
        diagnostics.insert(diagnostics.end(), loadDiagnostics.begin(), loadDiagnostics.end());
        return GtsModelRequestResult(std::move(resource), std::move(diagnostics));
    };
    auto reuse = [&](const Entry& entry)
    {
        diagnostics.insert(diagnostics.end(), entry.diagnostics.begin(), entry.diagnostics.end());
        return GtsModelRequestResult(entry.resource, std::move(diagnostics));
    };

    for (const auto& candidate : candidates)
    {
        const auto representation = fs::weakly_canonical(candidate, error);
        if (error)
            return failure("model.request.path", error.message());
        const auto cached = entries.find({identity, representation, true});
        const bool exists = fs::exists(candidate, error);
        if (error)
            return failure("model.request.path", error.message());
        if (!exists && cached == entries.end())
        {
            if (explicitCooked)
                return failure("model.cooked.missing", "Cooked model is missing: " + candidate.string());
            continue;
        }
        const auto& required = request.requiredCapabilities;
        // V1 cooked data is static. It has no manifest proving fidelity to a
        // glTF source, so it cannot replace that source's full definition.
        const bool staticCompatible = !required.skeletons && !required.skinBindings && !required.animations &&
                                      (!required.hierarchy || extensionOf(candidate) == ".gmodel") &&
                                      (explicitCooked || extension == ".obj");
        if (!staticCompatible)
        {
            if (explicitCooked)
                return failure("model.request.capabilities",
                               "Static cooked content cannot satisfy required model capabilities");
            diagnostics.push_back(
                {GtsModelDiagnosticSeverity::Warning,
                 "model.cooked.incompatible",
                 "Static cooked artifact cannot preserve the requested definition/capabilities; skipping " +
                     candidate.string(),
                 identity.string()});
            continue;
        }
        if (cached != entries.end())
        {
            if (cached->second.resource->capabilities().satisfies(required))
                return reuse(cached->second);
            if (explicitCooked)
                return failure("model.request.capabilities", "Cached cooked model lacks required capabilities");
            continue;
        }
        std::vector<GtsModelDiagnostic> loadDiagnostics;
        auto                            prepared = loadGtsCookedModel(representation, loadDiagnostics);
        if (!prepared)
        {
            diagnostics.insert(diagnostics.end(), loadDiagnostics.begin(), loadDiagnostics.end());
            return {{}, std::move(diagnostics)};
        }
        GtsModelHandle resource(new GtsModelResource(identity, std::move(*prepared)));
        if (resource->capabilities().satisfies(required))
            return publish(representation, std::move(resource), std::move(loadDiagnostics), true);
        if (explicitCooked)
            return failure("model.request.capabilities", "Cooked model lacks required capabilities");
        diagnostics.push_back({GtsModelDiagnosticSeverity::Warning,
                               "model.cooked.incompatible",
                               "Cooked definition lacks required capabilities: " + candidate.string(),
                               identity.string()});
    }
    if (!sourceAllowed)
    {
        return failure("model.request.cooked_required",
                       "Runtime asset policy requires compatible cooked model data for '" + identity.string() +
                           "'; no compatible artifact is available. Source fallback is forbidden (current cooked v1 "
                           "cannot store skeletons/animation).");
    }
    if (const auto cached = entries.find({identity, identity, false}); cached != entries.end())
    {
        if (!cached->second.resource->capabilities().satisfies(request.requiredCapabilities))
        {
            return failure("model.request.capabilities", "Cached model does not satisfy required capabilities");
        }
        return reuse(cached->second);
    }
    auto imported        = loadGtsModelSource(identity);
    auto loadDiagnostics = imported.diagnostics();
    if (!imported.succeeded())
    {
        diagnostics.insert(diagnostics.end(), loadDiagnostics.begin(), loadDiagnostics.end());
        return {{}, std::move(diagnostics)};
    }
    if (!imported.bundle()->model)
        return failure("model.request.no_model", "A model resource requires a primary model definition");
    const auto validation = validateGtsModelImportBundle(*imported.bundle());
    if (!validation.isValid())
    {
        diagnostics.insert(diagnostics.end(), validation.diagnostics.begin(), validation.diagnostics.end());
        return {{}, std::move(diagnostics)};
    }
    GtsModelHandle resource(new GtsModelResource(identity, *imported.bundle()));
    if (!resource->capabilities().satisfies(request.requiredCapabilities))
    {
        diagnostics.insert(diagnostics.end(), loadDiagnostics.begin(), loadDiagnostics.end());
        return failure(
            "model.request.capabilities",
            "Model does not satisfy required capabilities (geometry/hierarchy/skeletons/skin bindings/animations)");
    }
    return publish(identity, std::move(resource), std::move(loadDiagnostics), false);
}

const GtsModelResource* GtsModelRegistry::lookup(const GtsModelHandle& handle) const
{
    if (!handle)
        return nullptr;
    const auto first = entries.lower_bound({handle->identityPath(), {}, false});
    for (auto entry = first; entry != entries.end() && std::get<0>(entry->first) == handle->identityPath(); ++entry)
    {
        if (entry->second.resource == handle)
            return handle.get();
    }
    return nullptr;
}
