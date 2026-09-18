#ifdef NDEBUG
#undef NDEBUG
#endif

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

#include <stb_image_write.h>

#include "assets/cooking/AssetCooker.h"
#include "assets/loading/model/GtsModelRegistry.h"
#include "assets/loading/cooked/MaterialAssetLoader.h"
#include "MeshManager.hpp"
#include "assets/loading/cooked/TextureAssetLoader.h"
#include "assets/importer/obj/GtsObjModelImporter.h"
#include "assets/model/GtsModelAsset.h"
#include "assets/model/GtsModelImportResult.h"

namespace
{
    void write(const std::filesystem::path& path, const std::string& contents)
    {
        std::ofstream file(path);
        file << contents;
        assert(file.good());
    }

    std::vector<uint8_t> bytes(const std::filesystem::path& path)
    {
        std::ifstream file(path, std::ios::binary);
        assert(file.good());
        return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
    }

    void image(const std::filesystem::path& path, uint8_t r, uint8_t g, uint8_t b, int size = 2)
    {
        std::vector<uint8_t> pixels;
        for (int i = 0; i < size * size; ++i)
            pixels.insert(pixels.end(), {r, g, b, 255});
        assert(stbi_write_png(path.string().c_str(), size, size, 4, pixels.data(), size * 4));
    }

    void policy(const char* value)
    {
#ifdef _WIN32
        _putenv_s("GTS_RUNTIME_ASSET_POLICY", value);
#else
        setenv("GTS_RUNTIME_ASSET_POLICY", value, 1);
#endif
    }

    bool rejected(const std::filesystem::path& path)
    {
        MeshResource resource;
        try { MeshManager::loadMeshCpu(path.string(), resource); }
        catch (const std::runtime_error&) { return true; }
        return false;
    }

    bool diagnostic(const gts::rendering::AssetCookResult& result, const std::string& code)
    {
        return std::any_of(result.diagnostics.begin(), result.diagnostics.end(),
                           [&](const auto& item) { return item.code == code; });
    }
}

int main()
{
    using namespace gts::rendering;
    const auto root = std::filesystem::temp_directory_path() / "gravitas_canonical_obj_pipeline_test";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root / "mtl");
    const auto source = root / "mesh.obj";
    image(root / "mtl/color.png", 200, 80, 20);
    image(root / "mtl/metal.png", 12, 64, 230);
    image(root / "mtl/rough.png", 128, 17, 45);
    write(root / "mtl/surface.mtl",
          "newmtl red\nKd 0.8 0.4 0.2\nd 0.6\nPm 0.7\nPr 0.3\nKe 1 2 3\n"
          "map_Kd color.png\nmap_Pm -imfchan g metal.png\nmap_Pr rough.png\n"
          "map_Ka -imfchan b metal.png\nmap_Ke color.png\nnorm color.png\n"
          "newmtl blue\nKd 0 0 1\nmap_Kd color.png\n");
    const std::string geometry =
        "mtllib mtl/surface.mtl\n"
        "v 0 0 0 1 0 0\nv 1 0 0 0 1 0\nv 0 1 0 0 0 1\n"
        "vt 0 0.25\nvt 1 0.25\nvt 0 1\nvn 0 0 -2\n"
        "o body\nusemtl red\nf 1/1 2/2 3/3\n"
        "usemtl blue\nf 1/1/1 3/3/1 2/2/1\n"
        "usemtl red\nf 1/1 2/2 3/3\n"
        "o trim\nusemtl blue\nf 1/1 2/2 3/3\n";
    write(source, geometry);

    policy("strict");
    assert(rejected(source));
    policy("development");
    MeshResource runtime;
    MeshManager::loadMeshCpu(source.string(), runtime);
    assert(runtime.vertices.size() == 12 && runtime.indices.size() == 12);
    assert(runtime.submeshes.size() == 4);
    assert(runtime.metadata.generatedNormals && runtime.metadata.generatedTangents);
    assert(runtime.vertices[0].texCoord.y == 0.75f);
    assert(runtime.vertices[3].normal == glm::vec3(0, 0, -2));
    assert(runtime.vertices[0].color == glm::vec4(1, 0, 0, 1));
    assert(runtime.vertexBuffer == VK_NULL_HANDLE && runtime.indexBuffer == VK_NULL_HANDLE);
    for (uint32_t i = 0; i < 4; ++i)
    {
        assert(runtime.submeshes[i].firstIndex == i * 3 && runtime.submeshes[i].indexCount == 3);
        assert(runtime.indices[i * 3] == i * 3);
    }

    AssetCookerOptions options;
    options.outputDirectory = root;
    const auto cooked = AssetCooker::cookSourceAsset(source, options);
    for (const auto& d : cooked.diagnostics)
        if (d.severity == AssetDiagnosticSeverity::Error) std::fprintf(stderr, "%s: %s\n", d.code.c_str(), d.message.c_str());
    assert(cooked.succeeded());
    assert(cooked.meshes.size() == 2 && cooked.models.size() == 1 && cooked.materials.size() == 2);
    assert(cooked.meshes[0].submeshes.size() == 3);
    assert(cooked.meshes[0].bounds.valid && cooked.meshes[0].bounds.max == glm::vec3(1, 1, 0));
    assert(cooked.meshes[0].submeshes[0].material.id == cooked.meshes[0].submeshes[2].material.id);
    assert(cooked.meshes[0].submeshes[0].material.id != cooked.meshes[0].submeshes[1].material.id);
    assert(cooked.meshes[1].submeshes[0].debugName == "trim_primitive_0");
    const auto& red = cooked.materials[0];
    assert(red.baseColor == glm::vec4(0.8f, 0.4f, 0.2f, 0.6f));
    assert(red.metallic == 0.7f && red.roughness == 0.3f && red.emissiveFactor == glm::vec3(1, 2, 3));
    assert(red.renderState.alphaMode == MaterialAlphaMode::Blend && !red.renderState.depthWrite);
    assert(red.dependencies.size() == 5);
    assert(red.baseColorTexture.id == cooked.materials[1].baseColorTexture.id);
    assert(red.baseColorTexture.id != red.emissiveTexture.id);
    TextureAssetData packed;
    std::string error;
    assert(TextureAssetLoader::load(root / red.metallicRoughnessTexture.logicalPath, packed, &error));
    assert(packed.mips[0].bytes[1] == 128 && packed.mips[0].bytes[2] == 64);
    assert(packed.colorSpace == TextureColorSpace::Linear && packed.mipCount == 2);
    TextureAssetData ao;
    assert(TextureAssetLoader::load(root / red.ambientOcclusionTexture.logicalPath, ao, &error));
    assert(ao.mips[0].bytes[0] == 230);

    options.outputDirectory = root / "repeat";
    const auto repeated = AssetCooker::cookSourceAsset(source, options);
    assert(repeated.succeeded());
    for (const auto& output : cooked.outputs)
        assert(bytes(output.path) == bytes(options.outputDirectory / output.path.filename()));

    // Cooked preference works even when source parsing would now fail, then with no source at all.
    write(source, "f 99 98 97\n");
    policy("strict");
    MeshResource loaded;
    GtsModelRegistry registry;
    assert(registry.requestModel(source).succeeded());
    MeshManager::loadMeshCpu((root / cooked.models[0].meshes[0].logicalPath).string(), loaded);
    assert(loaded.submeshes[0].material.id == red.id);
    std::filesystem::remove(source);
    std::filesystem::remove_all(root / "mtl");
    assert(registry.requestModel(source).succeeded());
    MeshManager::loadMeshCpu((root / cooked.models[0].meshes[0].logicalPath).string(), loaded);
    for (const auto& material : cooked.materials)
        for (const auto& dependency : material.dependencies)
        {
            TextureAssetData texture;
            assert(TextureAssetLoader::load(root / dependency.logicalPath, texture, &error));
        }

    // Public runtime loading rejects malformed source; minimal missing attributes get prepared defaults.
    policy("development");
    write(root / "bad.obj", "v 0 0 0\nf 1 2 3\n");
    assert(rejected(root / "bad.obj"));
    write(root / "minimal.obj", "v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n");
    MeshManager::loadMeshCpu((root / "minimal.obj").string(), loaded);
    assert(loaded.metadata.generatedNormals && !loaded.metadata.generatedTangents);
    assert(loaded.vertices[0].color == glm::vec4(1) && loaded.vertices[0].texCoord == glm::vec2(0));
    options.vertexColorOnly = true;
    const auto minimal = AssetCooker::cookSourceAsset(root / "minimal.obj", options);
    assert(minimal.succeeded() && minimal.materials[0].shaderFamily == MaterialShaderFamily::Unlit);

    // Exercise v1 adaptation limits and independent channels without source-specific repairs.
    const auto imported = GtsObjModelImporter{}.importAsset({root / "minimal.obj"});
    assert(imported.succeeded());
    auto model = *imported.asset();
    model.nodes[0].localTransform[3][0] = 1;
    const auto transformed = AssetCooker::cookModelAsset(model, root / "transformed.obj", options);
    assert(transformed.succeeded() && transformed.models.size() == 1);
    assert(transformed.models[0].nodes[0].localTransform == model.nodes[0].localTransform);
    model = *imported.asset();
    model.materials.emplace_back();
    image(root / "scalar.png", 31, 63, 127);
    image(root / "other.png", 1, 2, 3, 1);
    model.images.push_back({"scalar", root / "scalar.png"});
    model.images.push_back({"other", root / "other.png"});
    auto& primitive = model.meshes[0].primitives[0];
    primitive.materialIndex = 0;
    primitive.attributes.push_back({GtsVertexSemantic::TexCoord, 0, std::vector<glm::vec2>(3)});
    auto& material = model.materials[0];
    material.metallicImage = GtsModelScalarImageBinding{{0, 0}, GtsModelTextureChannel::Blue};
    material.roughnessImage = GtsModelScalarImageBinding{{0, 0}, GtsModelTextureChannel::Green};
    material.alphaMode = GtsModelAlphaMode::Mask;
    material.alphaCutoff = 0.4f;
    material.doubleSided = true;
    options.vertexColorOnly = false;
    const auto shared = AssetCooker::cookModelAsset(model, root / "shared", options);
    assert(shared.succeeded());
    assert(shared.materials[0].renderState.doubleSided && shared.materials[0].renderState.alphaCutoff == 0.4f);
    assert(shared.textures[0].mips[0].bytes[1] == 63 && shared.textures[0].mips[0].bytes[2] == 127);
    material.roughnessImage.reset();
    const auto metalOnly = AssetCooker::cookModelAsset(model, root / "metal_only", options);
    assert(metalOnly.succeeded() && metalOnly.textures[0].mips[0].bytes[1] == 255);
    material.roughnessImage = GtsModelScalarImageBinding{{1, 0}, GtsModelTextureChannel::Red};
    assert(diagnostic(AssetCooker::cookModelAsset(model, root / "unequal", options), "ASSET_COOK_SCALAR_IMAGE_SIZE"));
    material.roughnessImage.reset();
    primitive.attributes.push_back({GtsVertexSemantic::TexCoord, 1, std::vector<glm::vec2>(3)});
    material.metallicImage->image.texCoordSet = 1;
    assert(diagnostic(AssetCooker::cookModelAsset(model, root / "uv1", options), "ASSET_COOK_UV_SET_UNSUPPORTED"));
    material.metallicImage->image.texCoordSet = 0;
    model.images[0].source = GtsModelEmbeddedImage{bytes(root / "scalar.png"), "image/png"};
    const auto embedded = AssetCooker::cookModelAsset(model, root / "embedded", options);
    assert(embedded.succeeded() && embedded.textures[0].mips[0].bytes[2] == 127);
    primitive.indices = {0, 1, 99};
    assert(AssetCooker::cookModelAsset(model, root / "invalid", options).hasErrors());
    write(root / "minimal.gmesh", "invalid cooked header");
    assert(rejected(root / "minimal.obj"));
    assert(rejected(root / "unsupported.glb"));
    policy("development");
    std::filesystem::remove_all(root);
    return 0;
}
