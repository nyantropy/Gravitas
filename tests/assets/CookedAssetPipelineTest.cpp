#ifdef NDEBUG
#undef NDEBUG
#endif

#include <cassert>
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "AssetCooker.h"
#include "MaterialAssetLoader.h"
#include "MeshAssetLoader.h"
#include "MeshManager.hpp"
#include "ModelAssetLoader.h"

namespace
{
    void writeTextFile(const std::filesystem::path& path, const std::string& text)
    {
        std::filesystem::create_directories(path.parent_path());
        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        assert(file);
        file << text;
    }

    std::vector<uint8_t> readBinaryFile(const std::filesystem::path& path)
    {
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        assert(file);
        const std::streamsize size = file.tellg();
        file.seekg(0, std::ios::beg);
        std::vector<uint8_t> bytes(static_cast<size_t>(size));
        file.read(reinterpret_cast<char*>(bytes.data()), size);
        return bytes;
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

    std::vector<std::filesystem::path> outputsOfType(
        const gts::rendering::AssetCookResult& result,
        gts::rendering::CookedAssetOutputType type)
    {
        std::vector<std::filesystem::path> paths;
        for (const gts::rendering::CookedAssetOutput& output : result.outputs)
        {
            if (output.type == type)
                paths.push_back(output.path);
        }
        std::sort(paths.begin(), paths.end());
        return paths;
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

    void appendU32(std::vector<uint8_t>& bytes, uint32_t value)
    {
        bytes.push_back(static_cast<uint8_t>(value & 0xFFu));
        bytes.push_back(static_cast<uint8_t>((value >> 8u) & 0xFFu));
        bytes.push_back(static_cast<uint8_t>((value >> 16u) & 0xFFu));
        bytes.push_back(static_cast<uint8_t>((value >> 24u) & 0xFFu));
    }

    void padToFour(std::vector<uint8_t>& bytes, uint8_t value)
    {
        while ((bytes.size() % 4u) != 0u)
            bytes.push_back(value);
    }

    std::vector<uint8_t> floatBytes(const std::vector<float>& values)
    {
        std::vector<uint8_t> bytes(values.size() * sizeof(float));
        std::memcpy(bytes.data(), values.data(), bytes.size());
        return bytes;
    }

    std::vector<uint8_t> u16Bytes(const std::vector<uint16_t>& values)
    {
        std::vector<uint8_t> bytes(values.size() * sizeof(uint16_t));
        std::memcpy(bytes.data(), values.data(), bytes.size());
        return bytes;
    }

    std::string joinJson(const std::vector<std::string>& values)
    {
        std::ostringstream stream;
        for (size_t i = 0; i < values.size(); ++i)
        {
            if (i != 0)
                stream << ',';
            stream << values[i];
        }
        return stream.str();
    }

    struct GltfDocument
    {
        std::vector<uint8_t> bin;
        std::vector<std::string> bufferViews;
        std::vector<std::string> accessors;

        int addBufferView(const std::vector<uint8_t>& bytes)
        {
            padToFour(bin, 0);
            const size_t offset = bin.size();
            bin.insert(bin.end(), bytes.begin(), bytes.end());

            std::ostringstream json;
            json << "{\"buffer\":0,\"byteOffset\":" << offset
                 << ",\"byteLength\":" << bytes.size() << "}";
            bufferViews.push_back(json.str());
            return static_cast<int>(bufferViews.size() - 1u);
        }

        int addAccessor(const std::vector<uint8_t>& bytes,
                        int componentType,
                        size_t count,
                        const std::string& type)
        {
            const int bufferView = addBufferView(bytes);
            std::ostringstream json;
            json << "{\"bufferView\":" << bufferView
                 << ",\"componentType\":" << componentType
                 << ",\"count\":" << count
                 << ",\"type\":\"" << type << "\"}";
            accessors.push_back(json.str());
            return static_cast<int>(accessors.size() - 1u);
        }
    };

    void writeGlb(const std::filesystem::path& path,
                  const std::string& json,
                  const std::vector<uint8_t>& bin)
    {
        std::vector<uint8_t> jsonBytes(json.begin(), json.end());
        padToFour(jsonBytes, static_cast<uint8_t>(' '));
        std::vector<uint8_t> binBytes = bin;
        padToFour(binBytes, 0);

        std::vector<uint8_t> bytes;
        const uint32_t totalLength =
            12u + 8u + static_cast<uint32_t>(jsonBytes.size())
            + (binBytes.empty() ? 0u : 8u + static_cast<uint32_t>(binBytes.size()));

        appendU32(bytes, 0x46546C67u);
        appendU32(bytes, 2u);
        appendU32(bytes, totalLength);
        appendU32(bytes, static_cast<uint32_t>(jsonBytes.size()));
        appendU32(bytes, 0x4E4F534Au);
        bytes.insert(bytes.end(), jsonBytes.begin(), jsonBytes.end());
        if (!binBytes.empty())
        {
            appendU32(bytes, static_cast<uint32_t>(binBytes.size()));
            appendU32(bytes, 0x004E4942u);
            bytes.insert(bytes.end(), binBytes.begin(), binBytes.end());
        }

        std::filesystem::create_directories(path.parent_path());
        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        assert(file);
        file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    }

    void writeStaticGlbFixture(const std::filesystem::path& path)
    {
        GltfDocument doc;
        const int positions = doc.addAccessor(
            floatBytes({
                0.0f, 0.0f, 0.0f,
                1.0f, 0.0f, 0.0f,
                0.0f, 1.0f, 0.0f,
                1.0f, 1.0f, 0.0f
            }),
            5126,
            4,
            "VEC3");
        const int normals = doc.addAccessor(
            floatBytes({
                0.0f, 0.0f, 1.0f,
                0.0f, 0.0f, 1.0f,
                0.0f, 0.0f, 1.0f,
                0.0f, 0.0f, 1.0f
            }),
            5126,
            4,
            "VEC3");
        const int uv0 = doc.addAccessor(
            floatBytes({
                0.0f, 0.0f,
                1.0f, 0.0f,
                0.0f, 1.0f,
                1.0f, 1.0f
            }),
            5126,
            4,
            "VEC2");
        const int indicesA = doc.addAccessor(u16Bytes({0, 1, 2}), 5123, 3, "SCALAR");
        const int indicesB = doc.addAccessor(u16Bytes({1, 3, 2}), 5123, 3, "SCALAR");

        std::ostringstream attributes;
        attributes << "{\"POSITION\":" << positions
                   << ",\"NORMAL\":" << normals
                   << ",\"TEXCOORD_0\":" << uv0 << "}";

        const std::string materials =
            "{\"name\":\"red\",\"pbrMetallicRoughness\":{\"baseColorFactor\":[1,0,0,1]}},"
            "{\"name\":\"blue\",\"pbrMetallicRoughness\":{\"baseColorFactor\":[0,0,1,1]}}";
        const std::string meshes =
            "{\"name\":\"body\",\"primitives\":["
            "{\"attributes\":" + attributes.str() + ",\"indices\":" + std::to_string(indicesA) + ",\"material\":0},"
            "{\"attributes\":" + attributes.str() + ",\"indices\":" + std::to_string(indicesB) + ",\"material\":1}]},"
            "{\"name\":\"head\",\"primitives\":["
            "{\"attributes\":" + attributes.str() + ",\"indices\":" + std::to_string(indicesA) + ",\"material\":1}]}";
        const std::string nodes =
            "{\"name\":\"body_node\",\"mesh\":0,\"translation\":[1,2,3],\"children\":[1]},"
            "{\"name\":\"head_node\",\"mesh\":1,\"scale\":[0.5,0.5,0.5]}";

        std::ostringstream json;
        json << "{\"asset\":{\"version\":\"2.0\"},"
             << "\"buffers\":[{\"byteLength\":" << doc.bin.size() << "}],"
             << "\"bufferViews\":[" << joinJson(doc.bufferViews) << "],"
             << "\"accessors\":[" << joinJson(doc.accessors) << "],"
             << "\"materials\":[" << materials << "],"
             << "\"meshes\":[" << meshes << "],"
             << "\"nodes\":[" << nodes << "]}";

        writeGlb(path, json.str(), doc.bin);
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

    const std::filesystem::path glbPath = sourceDirectory / "robot.glb";
    const std::filesystem::path gltfOutputA = root / "gltf_cooked_a";
    const std::filesystem::path gltfOutputB = root / "gltf_cooked_b";
    writeStaticGlbFixture(glbPath);

    options.outputDirectory = gltfOutputA;
    const gts::rendering::AssetCookResult gltfCookA =
        gts::rendering::AssetCooker::cookSourceAsset(glbPath, options);
    assert(!hasCookError(gltfCookA));
    assert(outputsOfType(gltfCookA, gts::rendering::CookedAssetOutputType::Mesh).size() == 2);
    assert(outputsOfType(gltfCookA, gts::rendering::CookedAssetOutputType::Material).size() == 2);

    const std::filesystem::path modelPath =
        findOutput(gltfCookA, gts::rendering::CookedAssetOutputType::Model);
    assert(!modelPath.empty());
    assert(std::filesystem::exists(modelPath));

    std::filesystem::remove(glbPath);

    gts::rendering::ModelAssetData modelAsset;
    assert(gts::rendering::ModelAssetLoader::load(modelPath, modelAsset, &error));
    assert(modelAsset.meshes.size() == 2);
    assert(modelAsset.materials.size() == 2);
    assert(modelAsset.nodes.size() == 2);
    assert(modelAsset.nodes[0].name == "body_node");
    assert(modelAsset.nodes[0].mesh.logicalPath == "robot_body.gmesh");
    assert(modelAsset.nodes[1].parentIndex == 0);
    assert(modelAsset.nodes[1].mesh.logicalPath == "robot_head.gmesh");

    gts::rendering::MeshAssetData gltfBodyMesh;
    assert(gts::rendering::MeshAssetLoader::load(gltfOutputA / "robot_body.gmesh", gltfBodyMesh, &error));
    assert(gltfBodyMesh.submeshes.size() == 2);
    assert(gltfBodyMesh.submeshes[0].firstIndex == 0);
    assert(gltfBodyMesh.submeshes[0].indexCount == 3);
    assert(gltfBodyMesh.submeshes[0].material.logicalPath == "robot_red.gmat");
    assert(gltfBodyMesh.submeshes[1].firstIndex == 3);
    assert(gltfBodyMesh.submeshes[1].indexCount == 3);
    assert(gltfBodyMesh.submeshes[1].material.logicalPath == "robot_blue.gmat");

    MeshResource gltfResource;
    MeshManager::populateMeshResourceFromAssetData(gltfResource, std::move(gltfBodyMesh));
    assert(gltfResource.submeshes.size() == 2);
    assert(gltfResource.metadata.vertexCount == 8);
    assert(gltfResource.metadata.indexCount == 6);

    writeStaticGlbFixture(glbPath);
    options.outputDirectory = gltfOutputB;
    const gts::rendering::AssetCookResult gltfCookB =
        gts::rendering::AssetCooker::cookSourceAsset(glbPath, options);
    assert(!hasCookError(gltfCookB));

    for (const char* filename : {
             "robot_body.gmesh",
             "robot_head.gmesh",
             "robot_red.gmat",
             "robot_blue.gmat",
             "robot.gmodel"})
    {
        assert(readBinaryFile(gltfOutputA / filename) == readBinaryFile(gltfOutputB / filename));
    }

    std::printf("CookedAssetPipelineTest passed\n");
    return 0;
}
