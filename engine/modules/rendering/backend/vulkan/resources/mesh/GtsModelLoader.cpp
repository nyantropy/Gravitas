#include "GtsModelLoader.hpp"

#include <sstream>
#include <stdexcept>
#include <utility>

#include "AssetCooker.h"
#include "ObjAssetImporter.h"

namespace
{
    std::string formatImportFailure(const std::string& modelPath,
                                    const gts::rendering::AssetImportResult& result)
    {
        std::ostringstream message;
        message << "Failed to import model " << modelPath;
        for (const gts::rendering::AssetDiagnostic& diagnostic : result.diagnostics)
        {
            if (diagnostic.severity != gts::rendering::AssetDiagnosticSeverity::Error)
                continue;

            message << "\n[" << diagnostic.code << "] " << diagnostic.message;
        }
        return message.str();
    }
}

MeshGeometryMetadata GtsModelLoader::loadModel(const std::string& modelPath,
                                               std::vector<Vertex>& vertices,
                                               std::vector<uint32_t>& indices)
{
    gts::rendering::ObjAssetImporter importer;
    gts::rendering::AssetImportRequest request;
    request.sourcePath = modelPath;

    gts::rendering::AssetImportResult result = importer.importAsset(request);
    if (!result.succeeded() || result.meshes.empty())
        throw std::runtime_error(formatImportFailure(modelPath, result));

    gts::rendering::MeshAssetData meshAsset =
        gts::rendering::cookImportedMesh(result.meshes.front());

    vertices = std::move(meshAsset.vertices);
    indices = std::move(meshAsset.indices);
    return gts::rendering::meshMetadataFromAssetData(meshAsset);
}
