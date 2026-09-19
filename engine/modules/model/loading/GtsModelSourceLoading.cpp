#include "GtsModelSourceLoading.h"

#include "GtsGltfModelImporter.h"
#include "GtsObjModelImporter.h"
#include "GtsModelImportResult.h"
#include "RuntimeAssetPolicy.h"

GtsModelImportResult loadGtsModelSource(const std::filesystem::path& path)
{
    if (gts::assets::runtimeSourceMeshFallbackSupported(path))
    {
        return GtsObjModelImporter{}.importAsset({path});
    }
    return GtsGltfModelImporter{}.importAsset({path});
}
