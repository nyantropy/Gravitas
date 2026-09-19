#include "model/loading/GtsModelSourceLoading.h"

#include "model/import/gltf/GtsGltfModelImporter.h"
#include "model/import/obj/GtsObjModelImporter.h"
#include "model/import/GtsModelImportResult.h"
#include "assets/loading/RuntimeAssetPolicy.h"

GtsModelImportResult loadGtsModelSource(const std::filesystem::path& path)
{
    if (gts::assets::runtimeSourceMeshFallbackSupported(path))
    {
        return GtsObjModelImporter{}.importAsset({path});
    }
    return GtsGltfModelImporter{}.importAsset({path});
}
