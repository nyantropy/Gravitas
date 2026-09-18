#include "assets/loading/model/GtsModelSourceLoading.h"

#include "assets/importer/gltf/GtsGltfModelImporter.h"
#include "assets/importer/obj/GtsObjModelImporter.h"
#include "assets/model/GtsModelImportResult.h"
#include "assets/loading/RuntimeAssetPolicy.h"

GtsModelImportResult loadGtsModelSource(const std::filesystem::path& path)
{
    if (gts::assets::runtimeSourceMeshFallbackSupported(path))
    {
        return GtsObjModelImporter{}.importAsset({path});
    }
    return GtsGltfModelImporter{}.importAsset({path});
}
