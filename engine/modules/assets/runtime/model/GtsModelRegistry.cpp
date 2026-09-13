#include "GtsModelRegistry.h"

#include <algorithm>
#include <cctype>
#include <string>
#include <utility>

#include "GtsModelResource.h"
#include "assets/importer/gltf/GtsGltfModelImporter.h"
#include "assets/importer/GtsModelImportBundleValidation.h"
#include "assets/model/GtsModelImportResult.h"

GtsModelRequestResult::GtsModelRequestResult(GtsModelHandle resource, std::vector<GtsModelDiagnostic> diagnostics)
    : resource(std::move(resource)), messages(std::move(diagnostics))
{
}

GtsModelRequestResult GtsModelRegistry::requestModel(const std::filesystem::path& path)
{
    auto failure = [&](const char* code, const std::string& message)
    {
        return GtsModelRequestResult({}, {{GtsModelDiagnosticSeverity::Error, code, message, path.string()}});
    };
    if (path.empty())
    {
        return failure("model.request.path", "Model request path is empty");
    }
    std::error_code error;
    const auto      absolute = std::filesystem::absolute(path, error);
    if (error)
    {
        return failure("model.request.path", error.message());
    }
    const auto normalized = std::filesystem::weakly_canonical(absolute, error);
    if (error)
    {
        return failure("model.request.path", error.message());
    }
    if (const auto cached = entries.find(normalized); cached != entries.end())
    {
        return {cached->second.resource, cached->second.diagnostics};
    }
    auto extension = normalized.extension().string();
    std::transform(extension.begin(),
                   extension.end(),
                   extension.begin(),
                   [](unsigned char c)
                   {
                       return static_cast<char>(std::tolower(c));
                   });
    if (extension != ".glb" && extension != ".gltf")
    {
        return failure(
            "model.request.unsupported",
            "Model registry currently supports canonical .glb/.gltf sources; other loading policies are pending");
    }
    // centralizes the existing development source path - cooked/source policy is a separate step
    auto imported    = GtsGltfModelImporter{}.importAsset({normalized.string()});
    auto diagnostics = imported.diagnostics();
    if (!imported.succeeded())
    {
        return {{}, std::move(diagnostics)};
    }
    if (!imported.bundle()->model)
    {
        diagnostics.push_back({GtsModelDiagnosticSeverity::Error,
                               "model.request.no_model",
                               "A model resource requires a primary model definition",
                               normalized.string()});
        return {{}, std::move(diagnostics)};
    }
    const auto validation = validateGtsModelImportBundle(*imported.bundle());
    if (!validation.isValid())
    {
        diagnostics.insert(diagnostics.end(), validation.diagnostics.begin(), validation.diagnostics.end());
        return {{}, std::move(diagnostics)};
    }
    GtsModelHandle resource(new GtsModelResource(normalized, *imported.bundle()));
    entries.emplace(normalized, Entry{resource, diagnostics});
    return {std::move(resource), std::move(diagnostics)};
}

const GtsModelResource* GtsModelRegistry::lookup(const GtsModelHandle& handle) const
{
    if (!handle)
    {
        return nullptr;
    }
    const auto found = entries.find(handle->sourcePath());
    return found != entries.end() && found->second.resource == handle ? handle.get() : nullptr;
}
