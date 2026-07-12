#ifdef NDEBUG
#undef NDEBUG
#endif

#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <utility>

#include "AssetCooker.h"
#include "MaterialAssetLoader.h"
#include "MeshAssetLoader.h"
#include "MeshManager.hpp"

namespace
{
    void writeTextFile(const std::filesystem::path& path, const std::string& text)
    {
        std::filesystem::create_directories(path.parent_path());
        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        assert(file);
        file << text;
    }

    std::filesystem::path findOutput(const gts::rendering::AssetCookResult& result,
                                     gts::rendering::CookedAssetOutputType type)
    {
        for (const gts::rendering::CookedAssetOutput& output : result.outputs)
        {
            if (output.type == type)
                return output.path;
        }
        return {};
    }

    bool hasCookError(const gts::rendering::AssetCookResult& result)
    {
        for (const gts::rendering::AssetDiagnostic& diagnostic : result.diagnostics)
        {
            if (diagnostic.severity == gts::rendering::AssetDiagnosticSeverity::Error)
                return true;
        }
        return false;
    }
}

int main()
{
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "gravitas_cooked_asset_pipeline_test";
    std::filesystem::remove_all(root);
    const std::filesystem::path sourceDirectory = root / "source";
    const std::filesystem::path outputDirectory = root / "cooked";
    const std::filesystem::path objPath = sourceDirectory / "fixture.obj";
    const std::filesystem::path mtlPath = sourceDirectory / "fixture.mtl";

    writeTextFile(
        mtlPath,
        "newmtl red\n"
        "Kd 1.0 0.0 0.0\n"
        "map_Kd red.png\n"
        "\n"
        "newmtl blue\n"
        "Kd 0.0 0.0 1.0\n");

    writeTextFile(
        objPath,
        "mtllib fixture.mtl\n"
        "o fixture\n"
        "v 0 0 0 1 0 0\n"
        "v 1 0 0 0 1 0\n"
        "v 0 1 0 0 0 1\n"
        "v 1 1 0 1 1 1\n"
        "vt 0 0\n"
        "vt 1 0\n"
        "vt 0 1\n"
        "vt 1 1\n"
        "vn 0 0 1\n"
        "usemtl red\n"
        "f 1/1/1 2/2/1 3/3/1\n"
        "usemtl blue\n"
        "f 2/2/1 4/4/1 3/3/1\n");

    gts::rendering::AssetCookerOptions options;
    options.outputDirectory = outputDirectory;
    const gts::rendering::AssetCookResult cookResult =
        gts::rendering::AssetCooker::cookSourceAsset(objPath, options);

    assert(!hasCookError(cookResult));
    const std::filesystem::path meshPath =
        findOutput(cookResult, gts::rendering::CookedAssetOutputType::Mesh);
    const std::filesystem::path materialPath =
        findOutput(cookResult, gts::rendering::CookedAssetOutputType::Material);
    assert(!meshPath.empty());
    assert(!materialPath.empty());
    assert(std::filesystem::exists(meshPath));
    assert(std::filesystem::exists(materialPath));

    std::filesystem::remove(objPath);

    gts::rendering::MeshAssetData meshAsset;
    std::string error;
    assert(gts::rendering::MeshAssetLoader::load(meshPath, meshAsset, &error));
    assert(meshAsset.vertices.size() == 4);
    assert(meshAsset.indices.size() == 6);
    assert(meshAsset.submeshes.size() == 2);
    assert(meshAsset.submeshes[0].indexCount == 3);
    assert(meshAsset.submeshes[1].indexCount == 3);
    assert(!meshAsset.submeshes[0].material.empty());

    MeshResource resource;
    MeshManager::populateMeshResourceFromAssetData(resource, std::move(meshAsset));
    assert(resource.vertices.size() == 4);
    assert(resource.indices.size() == 6);
    assert(resource.submeshes.size() == 2);
    assert(resource.metadata.vertexCount == 4);
    assert(resource.metadata.indexCount == 6);
    assert(resource.vertexBuffer == VK_NULL_HANDLE);
    assert(resource.indexBuffer == VK_NULL_HANDLE);

    gts::rendering::MaterialAssetData materialAsset;
    assert(gts::rendering::MaterialAssetLoader::load(materialPath, materialAsset, &error));
    assert(materialAsset.shaderFamily == MaterialShaderFamily::StandardSurface);

    gts::rendering::MaterialRuntime runtime;
    const MaterialInstanceHandle handle =
        gts::rendering::MaterialAssetLoader::loadIntoRuntime(materialPath, runtime, &error);
    assert(handle.id != 0);
    const MaterialInstance* instance = runtime.getInstance(handle);
    assert(instance != nullptr);
    assert(instance->baseColorTexture.path == "red.png");
    assert(instance->baseColorTexture.colorSpace == TextureColorSpace::SRgb);

    std::printf("CookedAssetPipelineTest passed\n");
    return 0;
}
