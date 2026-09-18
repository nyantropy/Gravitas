#include "RuntimeMeshLoading.h"

#include <stdexcept>
#include <utility>

#include "MeshAssetLoader.h"
#include "assets/importer/obj/GtsObjModelImporter.h"
#include "assets/model/GtsModelImportResult.h"
#include "assets/realization/GtsStaticModelRealization.h"

namespace gts::rendering
{
std::filesystem::path resolveRuntimeMeshPath(const std::filesystem::path& path)
{
    if (gts::assets::isCookedMeshAssetPath(path))
        return path;
    const auto cookedPath = gts::assets::expectedCookedMeshAssetPath(path);
    if (std::filesystem::exists(cookedPath))
        return cookedPath;
    if (!gts::assets::runtimeSourceAssetFallbackAllowed())
        throw std::runtime_error("Cooked mesh asset is required by the runtime asset policy: " + cookedPath.string());
    if (!gts::assets::runtimeSourceMeshFallbackSupported(path))
        throw std::runtime_error("No runtime source loader exists for: " + path.string());
    return path;
}

MeshAssetData loadRuntimeMeshAsset(const std::filesystem::path& requestedPath,
                                  std::vector<GtsModelDiagnostic>& diagnostics)
{
    const auto path = resolveRuntimeMeshPath(requestedPath);
    if (gts::assets::isCookedMeshAssetPath(path))
    {
        MeshAssetData mesh;
        std::string error;
        if (!MeshAssetLoader::load(path, mesh, &error))
            throw std::runtime_error("Failed to load cooked mesh '" + path.string() + "': " + error);
        return mesh;
    }

    const auto imported = GtsObjModelImporter{}.importAsset({path});
    diagnostics.insert(diagnostics.end(), imported.diagnostics().begin(), imported.diagnostics().end());
    if (imported.succeeded())
    {
        auto mesh = realizeGtsFlatStaticModel(*imported.asset(), diagnostics);
        if (mesh)
        {
            mesh->debugName = path.stem().string();
            return std::move(*mesh);
        }
    }
    std::string error = "Failed to prepare source mesh '" + path.string() + "'";
    for (const auto& diagnostic : diagnostics)
        error += "\n" + diagnostic.code + ": " + diagnostic.message;
    throw std::runtime_error(error);
}
}
