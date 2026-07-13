#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "GltfAssetImporter.h"

namespace
{
    namespace rendering = gts::rendering;

    class TempAssetDirectory
    {
    public:
        TempAssetDirectory()
        {
            const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
            root = std::filesystem::temp_directory_path() /
                ("gravitas_gltf_importer_test_" + std::to_string(stamp));
            std::filesystem::create_directories(root);
        }

        ~TempAssetDirectory()
        {
            std::error_code ignored;
            std::filesystem::remove_all(root, ignored);
        }

        std::filesystem::path path(const std::string& relativePath) const
        {
            return root / relativePath;
        }

        void writeBytes(const std::string& relativePath,
                        const std::vector<uint8_t>& bytes) const
        {
            const std::filesystem::path output = path(relativePath);
            std::filesystem::create_directories(output.parent_path());
            std::ofstream file(output, std::ios::binary | std::ios::trunc);
            file.write(
                reinterpret_cast<const char*>(bytes.data()),
                static_cast<std::streamsize>(bytes.size()));
        }

    private:
        std::filesystem::path root;
    };

    bool require(bool condition, const std::string& message)
    {
        if (condition)
            return true;

        std::fprintf(stderr, "FAIL: %s\n", message.c_str());
        return false;
    }

    bool near(float lhs, float rhs, float epsilon = 0.0001f)
    {
        return std::abs(lhs - rhs) <= epsilon;
    }

    bool nearVec3(const glm::vec3& lhs, const glm::vec3& rhs, float epsilon = 0.0001f)
    {
        return near(lhs.x, rhs.x, epsilon)
            && near(lhs.y, rhs.y, epsilon)
            && near(lhs.z, rhs.z, epsilon);
    }

    bool nearVec4(const glm::vec4& lhs, const glm::vec4& rhs, float epsilon = 0.0001f)
    {
        return near(lhs.x, rhs.x, epsilon)
            && near(lhs.y, rhs.y, epsilon)
            && near(lhs.z, rhs.z, epsilon)
            && near(lhs.w, rhs.w, epsilon);
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

    struct GltfBinaryDocument
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
                        const std::string& type,
                        bool normalized = false)
        {
            const int bufferView = addBufferView(bytes);
            std::ostringstream json;
            json << "{\"bufferView\":" << bufferView
                 << ",\"componentType\":" << componentType
                 << ",\"count\":" << count
                 << ",\"type\":\"" << type << "\"";
            if (normalized)
                json << ",\"normalized\":true";
            json << "}";
            accessors.push_back(json.str());
            return static_cast<int>(accessors.size() - 1u);
        }
    };

    struct BasicAccessors
    {
        int positions = -1;
        int normals = -1;
        int tangents = -1;
        int uv0 = -1;
        int colors = -1;
        int indicesA = -1;
        int indicesB = -1;
    };

    BasicAccessors addBasicAccessors(GltfBinaryDocument& doc)
    {
        BasicAccessors accessors;
        accessors.positions = doc.addAccessor(
            floatBytes({
                0.0f, 0.0f, 0.0f,
                1.0f, 0.0f, 0.0f,
                0.0f, 1.0f, 0.0f,
                1.0f, 1.0f, 0.0f
            }),
            5126,
            4,
            "VEC3");
        accessors.normals = doc.addAccessor(
            floatBytes({
                0.0f, 0.0f, 1.0f,
                0.0f, 0.0f, 1.0f,
                0.0f, 0.0f, 1.0f,
                0.0f, 0.0f, 1.0f
            }),
            5126,
            4,
            "VEC3");
        accessors.tangents = doc.addAccessor(
            floatBytes({
                1.0f, 0.0f, 0.0f, 1.0f,
                1.0f, 0.0f, 0.0f, 1.0f,
                1.0f, 0.0f, 0.0f, 1.0f,
                1.0f, 0.0f, 0.0f, 1.0f
            }),
            5126,
            4,
            "VEC4");
        accessors.uv0 = doc.addAccessor(
            floatBytes({
                0.0f, 0.0f,
                1.0f, 0.0f,
                0.0f, 1.0f,
                1.0f, 1.0f
            }),
            5126,
            4,
            "VEC2");
        accessors.colors = doc.addAccessor(
            floatBytes({
                1.0f, 0.0f, 0.0f, 1.0f,
                0.0f, 1.0f, 0.0f, 1.0f,
                0.0f, 0.0f, 1.0f, 1.0f,
                1.0f, 1.0f, 1.0f, 1.0f
            }),
            5126,
            4,
            "VEC4");
        accessors.indicesA = doc.addAccessor(u16Bytes({0, 1, 2}), 5123, 3, "SCALAR");
        accessors.indicesB = doc.addAccessor(u16Bytes({1, 3, 2}), 5123, 3, "SCALAR");
        return accessors;
    }

    std::string fullAttributeJson(const BasicAccessors& accessors)
    {
        std::ostringstream json;
        json << "{\"POSITION\":" << accessors.positions
             << ",\"NORMAL\":" << accessors.normals
             << ",\"TANGENT\":" << accessors.tangents
             << ",\"TEXCOORD_0\":" << accessors.uv0
             << ",\"COLOR_0\":" << accessors.colors << "}";
        return json.str();
    }

    std::string positionOnlyAttributeJson(const BasicAccessors& accessors)
    {
        return "{\"POSITION\":" + std::to_string(accessors.positions) + "}";
    }

    std::string makeJson(const GltfBinaryDocument& doc,
                         const std::string& meshes,
                         const std::string& materials = {},
                         const std::string& textures = {},
                         const std::string& images = {},
                         const std::string& nodes = {},
                         const std::string& extraTopLevel = {})
    {
        std::ostringstream json;
        json << "{\"asset\":{\"version\":\"2.0\"}";
        if (!doc.bin.empty() || !doc.bufferViews.empty() || !doc.accessors.empty())
        {
            json << ",\"buffers\":[{\"byteLength\":" << doc.bin.size() << "}]"
                 << ",\"bufferViews\":[" << joinJson(doc.bufferViews) << "]"
                 << ",\"accessors\":[" << joinJson(doc.accessors) << "]";
        }
        if (!images.empty())
            json << ",\"images\":[" << images << "]";
        if (!textures.empty())
            json << ",\"textures\":[" << textures << "]";
        if (!materials.empty())
            json << ",\"materials\":[" << materials << "]";
        if (!meshes.empty())
            json << ",\"meshes\":[" << meshes << "]";
        if (!nodes.empty())
            json << ",\"nodes\":[" << nodes << "]";
        if (!extraTopLevel.empty())
            json << ',' << extraTopLevel;
        json << "}";
        return json.str();
    }

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
            12u
            + 8u
            + static_cast<uint32_t>(jsonBytes.size())
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
        file.write(
            reinterpret_cast<const char*>(bytes.data()),
            static_cast<std::streamsize>(bytes.size()));
    }

    rendering::AssetImportResult importGltf(const std::filesystem::path& path)
    {
        rendering::GltfAssetImporter importer;
        rendering::AssetImportRequest request;
        request.sourcePath = path;
        request.requestedCapabilities = rendering::AssetImportCapability::All;
        return importer.importAsset(request);
    }

    bool hasDiagnostic(const rendering::AssetImportResult& result,
                       const std::string& code)
    {
        for (const rendering::AssetDiagnostic& diagnostic : result.diagnostics)
        {
            if (diagnostic.code == code)
                return true;
        }
        return false;
    }

    bool hasDependency(const rendering::AssetImportResult& result,
                       const std::filesystem::path& path,
                       rendering::AssetDependencyType type)
    {
        const std::filesystem::path normalized = path.lexically_normal();
        for (const rendering::AssetDependency& dependency : result.dependencies)
        {
            if (dependency.type == type && dependency.path == normalized)
                return true;
        }
        return false;
    }

    bool attributesMaterialsAndTexturesImport()
    {
        TempAssetDirectory temp;
        temp.writeBytes("base.png", {1, 2, 3});
        temp.writeBytes("mr.png", {4, 5, 6});
        temp.writeBytes("normal.png", {7, 8, 9});
        temp.writeBytes("ao.png", {10, 11, 12});

        GltfBinaryDocument doc;
        const BasicAccessors accessors = addBasicAccessors(doc);
        const int embeddedImageView = doc.addBufferView({
            0x89u, 0x50u, 0x4Eu, 0x47u, 0x0Du, 0x0Au, 0x1Au, 0x0Au
        });

        const std::string images =
            "{\"uri\":\"base.png\",\"mimeType\":\"image/png\",\"name\":\"baseTex\"},"
            "{\"uri\":\"mr.png\",\"mimeType\":\"image/png\",\"name\":\"mrTex\"},"
            "{\"uri\":\"normal.png\",\"mimeType\":\"image/png\",\"name\":\"normalTex\"},"
            "{\"uri\":\"ao.png\",\"mimeType\":\"image/png\",\"name\":\"aoTex\"},"
            "{\"bufferView\":" + std::to_string(embeddedImageView)
            + ",\"mimeType\":\"image/png\",\"name\":\"emissiveTex\"}";
        const std::string textures =
            "{\"source\":0},{\"source\":1},{\"source\":2},{\"source\":3},{\"source\":4}";
        const std::string materials =
            "{\"name\":\"matA\","
            "\"pbrMetallicRoughness\":{\"baseColorFactor\":[0.2,0.4,0.6,0.8],"
            "\"metallicFactor\":0.5,\"roughnessFactor\":0.25,"
            "\"baseColorTexture\":{\"index\":0},\"metallicRoughnessTexture\":{\"index\":1}},"
            "\"normalTexture\":{\"index\":2,\"scale\":0.7},"
            "\"occlusionTexture\":{\"index\":3,\"strength\":0.6},"
            "\"emissiveTexture\":{\"index\":4},"
            "\"emissiveFactor\":[0.1,0.2,0.3],"
            "\"extensions\":{\"KHR_materials_emissive_strength\":{\"emissiveStrength\":2.5}},"
            "\"alphaMode\":\"BLEND\",\"doubleSided\":true},"
            "{\"name\":\"matB\",\"alphaMode\":\"MASK\",\"alphaCutoff\":0.25}";
        const std::string meshes =
            "{\"name\":\"textured\",\"primitives\":[{\"attributes\":"
            + fullAttributeJson(accessors)
            + ",\"indices\":" + std::to_string(accessors.indicesA)
            + ",\"material\":0}]}";
        const std::string json = makeJson(
            doc,
            meshes,
            materials,
            textures,
            images,
            {},
            "\"extensionsUsed\":[\"KHR_materials_emissive_strength\"]");

        const std::filesystem::path path = temp.path("textured.glb");
        writeGlb(path, json, doc.bin);
        const rendering::AssetImportResult result = importGltf(path);
        if (!require(result.succeeded(), "textured GLB import succeeds"))
            return false;

        const rendering::ImportedMesh& mesh = result.meshes.front();
        const rendering::ImportedMaterial& material = result.materials.front();
        bool ok = require(result.meshes.size() == 1, "one glTF mesh imports")
            && require(result.materials.size() == 2, "two glTF materials import")
            && require(result.textures.size() == 5, "five glTF textures import")
            && require(mesh.vertices.size() == 4, "attribute import keeps vertex count")
            && require(mesh.indices.size() == 3, "index import keeps index count")
            && require(mesh.primitives.size() == 1, "primitive boundary imports")
            && require(mesh.primitives.front().materialIndex == 0, "primitive material index imports")
            && require(mesh.primitives.front().materialName == "matA", "primitive material name imports")
            && require(hasVertexAttribute(mesh.sourceAttributes, VertexAttributeFlags::Normal), "normal source attribute imports")
            && require(hasVertexAttribute(mesh.sourceAttributes, VertexAttributeFlags::Tangent), "tangent source attribute imports")
            && require(hasVertexAttribute(mesh.sourceAttributes, VertexAttributeFlags::UV0), "UV0 source attribute imports")
            && require(nearVec3(mesh.vertices[0].normal, {0.0f, 0.0f, 1.0f}), "normal value imports")
            && require(near(mesh.vertices[0].tangent.w, -1.0f), "tangent handedness follows V flip")
            && require(near(mesh.vertices[0].texCoord.y, 1.0f), "UV V is flipped")
            && require(near(mesh.vertices[2].texCoord.y, 0.0f), "UV V flip handles one")
            && require(nearVec4(mesh.vertices[1].color, {0.0f, 1.0f, 0.0f, 1.0f}), "vertex color imports")
            && require(nearVec4(material.baseColor.value_or(glm::vec4{}), {0.2f, 0.4f, 0.6f, 0.8f}), "base color factor imports")
            && require(near(material.metallic.value_or(0.0f), 0.5f), "metallic factor imports")
            && require(near(material.roughness.value_or(0.0f), 0.25f), "roughness factor imports")
            && require(material.baseColorTextureIndex == 0, "base-color texture index imports")
            && require(material.metallicRoughnessTextureIndex == 1, "metallic-roughness texture index imports")
            && require(material.normalTextureIndex == 2, "normal texture index imports")
            && require(material.ambientOcclusionTextureIndex == 3, "AO texture index imports")
            && require(material.emissiveTextureIndex == 4, "emissive texture index imports")
            && require(near(material.normalScale.value_or(0.0f), 0.7f), "normal scale imports")
            && require(near(material.ambientOcclusionStrength.value_or(0.0f), 0.6f), "AO strength imports")
            && require(near(material.emissiveStrength.value_or(0.0f), 2.5f), "emissive strength imports")
            && require(material.renderState.alphaMode == MaterialAlphaMode::Blend, "alpha blend imports")
            && require(material.renderState.doubleSided, "double-sided imports")
            && require(!material.renderState.depthWrite, "blend material disables depth write")
            && require(result.materials[1].renderState.alphaMode == MaterialAlphaMode::Mask, "alpha mask imports")
            && require(result.textures[0].logicalPath == "base.png", "external texture logical path imports")
            && require(result.textures[0].colorSpace == TextureColorSpace::SRgb, "base texture role sets sRGB")
            && require(result.textures[1].colorSpace == TextureColorSpace::Linear, "MR texture role sets linear")
            && require(result.textures[4].embedded(), "embedded texture bytes import")
            && require(result.textures[4].logicalPath == "textured_image_4.png", "embedded texture gets logical path")
            && require(hasDependency(result, temp.path("base.png"), rendering::AssetDependencyType::Texture), "external texture dependency records");
        return ok;
    }

    bool multipleMeshesPrimitivesAndNodesImport()
    {
        TempAssetDirectory temp;
        GltfBinaryDocument doc;
        const BasicAccessors accessors = addBasicAccessors(doc);
        const std::string materials = "{\"name\":\"matA\"},{\"name\":\"matB\"}";
        const std::string attributes = fullAttributeJson(accessors);
        const std::string meshes =
            "{\"name\":\"body\",\"primitives\":["
            "{\"attributes\":" + attributes + ",\"indices\":" + std::to_string(accessors.indicesA) + ",\"material\":0},"
            "{\"attributes\":" + attributes + ",\"indices\":" + std::to_string(accessors.indicesB) + ",\"material\":1}]},"
            "{\"name\":\"head\",\"primitives\":["
            "{\"attributes\":" + attributes + ",\"indices\":" + std::to_string(accessors.indicesA) + ",\"material\":1}]}";
        const std::string nodes =
            "{\"name\":\"root\",\"mesh\":0,\"translation\":[1,2,3],\"children\":[1]},"
            "{\"name\":\"child\",\"mesh\":1,\"scale\":[2,2,2]}";

        const std::filesystem::path path = temp.path("model.glb");
        writeGlb(path, makeJson(doc, meshes, materials, {}, {}, nodes), doc.bin);
        const rendering::AssetImportResult result = importGltf(path);
        if (!require(result.succeeded(), "multi-mesh GLB import succeeds"))
            return false;

        bool ok = require(result.meshes.size() == 2, "multiple glTF meshes import")
            && require(result.meshes[0].primitives.size() == 2, "multiple glTF primitives import")
            && require(result.meshes[0].primitives[0].firstIndex == 0, "first primitive starts at first index")
            && require(result.meshes[0].primitives[0].indexCount == 3, "first primitive range imports")
            && require(result.meshes[0].primitives[1].firstIndex == 3, "second primitive first index imports")
            && require(result.meshes[0].primitives[1].indexCount == 3, "second primitive range imports")
            && require(result.meshes[0].primitives[1].materialName == "matB", "second primitive material imports")
            && require(result.nodes.size() == 2, "glTF nodes import")
            && require(result.nodes[0].name == "root", "node name imports")
            && require(result.nodes[0].meshIndex == 0, "root node mesh index imports")
            && require(result.nodes[1].parentIndex == 0, "node hierarchy imports")
            && require(result.nodes[1].meshIndex == 1, "child node mesh index imports")
            && require(near(result.nodes[0].localTransform[3][0], 1.0f), "node translation X imports")
            && require(near(result.nodes[0].localTransform[3][1], 2.0f), "node translation Y imports")
            && require(near(result.nodes[0].localTransform[3][2], 3.0f), "node translation Z imports")
            && require(near(result.nodes[1].localTransform[0][0], 2.0f), "node scale imports");
        return ok;
    }

    bool missingDataAndTextureDiagnostics()
    {
        TempAssetDirectory temp;
        GltfBinaryDocument doc;
        const BasicAccessors accessors = addBasicAccessors(doc);
        const std::string images = "{\"uri\":\"missing.png\",\"mimeType\":\"image/png\"}";
        const std::string textures = "{\"source\":0}";
        const std::string materials =
            "{\"name\":\"missingTex\","
            "\"pbrMetallicRoughness\":{\"baseColorTexture\":{\"index\":0}}}";
        const std::string meshes =
            "{\"name\":\"missingData\",\"primitives\":[{\"attributes\":"
            + positionOnlyAttributeJson(accessors)
            + ",\"indices\":" + std::to_string(accessors.indicesA)
            + ",\"material\":0}]}";

        const std::filesystem::path path = temp.path("missing.glb");
        writeGlb(path, makeJson(doc, meshes, materials, textures, images), doc.bin);
        const rendering::AssetImportResult result = importGltf(path);

        return require(result.succeeded(), "missing optional glTF data imports with warnings")
            && require(result.meshes.front().hadMissingNormals, "missing normals are recorded")
            && require(result.meshes.front().hadMissingTexCoords, "missing UV0 is recorded")
            && require(hasDiagnostic(result, "GLTF_MISSING_NORMALS"), "missing normals diagnostic emits")
            && require(hasDiagnostic(result, "GLTF_MISSING_TANGENTS"), "missing tangents diagnostic emits")
            && require(hasDiagnostic(result, "GLTF_MISSING_UV0"), "missing UV0 diagnostic emits")
            && require(hasDiagnostic(result, "ASSET_DEPENDENCY_MISSING"), "missing texture diagnostic emits");
    }

    bool unsupportedFeatureDiagnostics()
    {
        TempAssetDirectory temp;
        GltfBinaryDocument doc;
        const BasicAccessors accessors = addBasicAccessors(doc);
        const std::string attributes =
            "{\"POSITION\":" + std::to_string(accessors.positions)
            + ",\"TEXCOORD_1\":" + std::to_string(accessors.uv0)
            + ",\"JOINTS_0\":" + std::to_string(accessors.colors)
            + ",\"WEIGHTS_0\":" + std::to_string(accessors.colors) + "}";
        const std::string meshes =
            "{\"name\":\"unsupported\",\"primitives\":[{\"attributes\":"
            + attributes
            + ",\"indices\":" + std::to_string(accessors.indicesA)
            + ",\"targets\":[{\"POSITION\":" + std::to_string(accessors.positions) + "}]}]}";
        const std::string nodes = "{\"name\":\"cameraNode\",\"mesh\":0,\"camera\":0,\"skin\":0}";
        const std::string extra =
            "\"extensionsUsed\":[\"KHR_lights_punctual\",\"EXT_unused\"],"
            "\"skins\":[{}],\"animations\":[{}],\"cameras\":[{}]";

        const std::filesystem::path path = temp.path("unsupported.glb");
        writeGlb(path, makeJson(doc, meshes, {}, {}, {}, nodes, extra), doc.bin);
        const rendering::AssetImportResult result = importGltf(path);

        return require(result.succeeded(), "unsupported optional glTF features warn without fatal error")
            && require(hasDiagnostic(result, "GLTF_LIGHT_SKIPPED"), "light diagnostic emits")
            && require(hasDiagnostic(result, "GLTF_UNSUPPORTED_EXTENSION"), "optional extension diagnostic emits")
            && require(hasDiagnostic(result, "GLTF_SKINNING_NOT_IMPLEMENTED"), "skinning diagnostic emits")
            && require(hasDiagnostic(result, "GLTF_ANIMATION_SKIPPED"), "animation diagnostic emits")
            && require(hasDiagnostic(result, "GLTF_CAMERA_SKIPPED"), "camera diagnostic emits")
            && require(hasDiagnostic(result, "GLTF_MULTIPLE_UV_SETS_IGNORED"), "extra UV diagnostic emits")
            && require(hasDiagnostic(result, "GLTF_MORPH_TARGETS_NOT_IMPLEMENTED"), "morph target diagnostic emits");
    }

    bool requiredUnsupportedExtensionFails()
    {
        TempAssetDirectory temp;
        const std::filesystem::path path = temp.path("required.glb");
        writeGlb(
            path,
            "{\"asset\":{\"version\":\"2.0\"},\"extensionsRequired\":[\"KHR_draco_mesh_compression\"]}",
            {});

        const rendering::AssetImportResult result = importGltf(path);
        return require(!result.succeeded(), "unsupported required extension fails")
            && require(hasDiagnostic(result, "GLTF_UNSUPPORTED_EXTENSION"), "required extension diagnostic emits");
    }

    bool invalidAndMissingDiagnostics()
    {
        TempAssetDirectory temp;
        const rendering::AssetImportResult missing = importGltf(temp.path("missing.glb"));
        bool ok = require(!missing.succeeded(), "missing GLB fails")
            && require(hasDiagnostic(missing, "ASSET_SOURCE_MISSING"), "missing source diagnostic emits");

        const std::filesystem::path invalidPath = temp.path("invalid.glb");
        temp.writeBytes("invalid.glb", {'b', 'a', 'd'});
        const rendering::AssetImportResult invalid = importGltf(invalidPath);
        ok = require(!invalid.succeeded(), "invalid GLB fails") && ok;
        ok = require(hasDiagnostic(invalid, "GLTF_READ_FAILED"), "invalid GLB diagnostic emits") && ok;
        return ok;
    }

    bool registrySelection()
    {
        rendering::AssetImporterRegistry registry;
        registry.registerImporter(std::make_unique<rendering::GltfAssetImporter>());

        const bool glbSelected =
            registry.findImporter("mesh.glb") != nullptr;
        const bool gltfSelected =
            registry.findImporter("mesh.gltf") != nullptr;
        const bool explicitSelected =
            registry.findImporter(
                "mesh.asset",
                "gltf",
                rendering::AssetImportCapability::Meshes | rendering::AssetImportCapability::Nodes) != nullptr;
        const bool allCapabilitiesSelected =
            registry.findImporter(
                "mesh.glb",
                {},
                rendering::AssetImportCapability::All) != nullptr;

        return require(glbSelected, "registry selects glTF importer by .glb")
            && require(gltfSelected, "registry selects glTF importer by .gltf")
            && require(explicitSelected, "registry supports explicit glTF importer")
            && require(allCapabilitiesSelected, "registry accepts all glTF importer capabilities");
    }
}

int main()
{
    bool ok = true;
    ok = attributesMaterialsAndTexturesImport() && ok;
    ok = multipleMeshesPrimitivesAndNodesImport() && ok;
    ok = missingDataAndTextureDiagnostics() && ok;
    ok = unsupportedFeatureDiagnostics() && ok;
    ok = requiredUnsupportedExtensionFails() && ok;
    ok = invalidAndMissingDiagnostics() && ok;
    ok = registrySelection() && ok;

    if (!ok)
        return 1;

    std::printf("GltfAssetImporterTest passed\n");
    return 0;
}
