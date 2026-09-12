#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "GtsGltfModelImporter.h"
#include "GtsJsonParser.h"
#include "assets/model/GtsModelImportResult.h"

namespace
{
    using Json  = GtsJsonValue;
    using Array = Json::Array;
    void require(bool condition, const std::string& message)
    {
        if (!condition)
            throw std::runtime_error(message);
    }
    Json parse(const std::string& text)
    {
        Json        value;
        std::string error;
        require(GtsJsonParser::parse(text, value, &error), error);
        return value;
    }
    Json& field(Json& value, const std::string& name)
    {
        for (auto& [key, child] : std::get<Json::Object>(value.value))
            if (key == name)
                return child;
        std::get<Json::Object>(value.value).emplace_back(name, Json{});
        return std::get<Json::Object>(value.value).back().second;
    }
    void erase(Json& value, const std::string& name)
    {
        auto& fields = std::get<Json::Object>(value.value);
        std::erase_if(fields,
                      [&](const auto& entry)
                      {
                          return entry.first == name;
                      });
    }
    Json& at(Json& value, size_t index)
    {
        return std::get<Array>(value.value).at(index);
    }
    void append32(std::vector<uint8_t>& bytes, uint32_t value)
    {
        for (size_t i = 0; i < 4; ++i)
            bytes.push_back(static_cast<uint8_t>(value >> (i * 8)));
    }
    std::vector<uint8_t> floats(std::initializer_list<float> values)
    {
        std::vector<uint8_t> bytes;
        for (float value : values)
            append32(bytes, std::bit_cast<uint32_t>(value));
        return bytes;
    }
    std::string base64(const std::vector<uint8_t>& bytes)
    {
        constexpr char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        std::string    result;
        for (size_t i = 0; i < bytes.size(); i += 3)
        {
            const uint32_t value = (uint32_t(bytes[i]) << 16) |
                                   (i + 1 < bytes.size() ? uint32_t(bytes[i + 1]) << 8 : 0) |
                                   (i + 2 < bytes.size() ? bytes[i + 2] : 0);
            result += alphabet[value >> 18];
            result += alphabet[(value >> 12) & 63];
            result += i + 1 < bytes.size() ? alphabet[(value >> 6) & 63] : '=';
            result += i + 2 < bytes.size() ? alphabet[value & 63] : '=';
        }
        return result;
    }
    void write(const std::filesystem::path& path, const std::vector<uint8_t>& bytes)
    {
        std::ofstream file(path, std::ios::binary);
        file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        require(file.good(), "fixture write");
    }
    struct Fixture
    {
        Json root = parse(R"({"asset":{"version":"2.0"},"buffers":[{}],"bufferViews":[],"accessors":[],
        "meshes":[{"name":"triangle","primitives":[{"attributes":{}}]}],
        "nodes":[{"name":"root","mesh":0}],"scenes":[{"nodes":[0]}],"scene":0})");
        std::vector<uint8_t> bin;
        Fixture()
        {
            stream("POSITION", floats({0, 0, 0, 1, 0, 0, 0, 1, 0}), "VEC3", 5126);
        }
        Json& primitive()
        {
            return at(field(at(field(root, "meshes"), 0), "primitives"), 0);
        }
        Json& attributes()
        {
            return field(primitive(), "attributes");
        }
        uint32_t view(const std::vector<uint8_t>& bytes, uint32_t stride = 0)
        {
            while (bin.size() % 4)
                bin.push_back(0);
            Json value                 = parse("{}");
            field(value, "buffer")     = 0u;
            field(value, "byteOffset") = uint32_t(bin.size());
            field(value, "byteLength") = uint32_t(bytes.size());
            if (stride)
                field(value, "byteStride") = stride;
            auto& views = std::get<Array>(field(root, "bufferViews").value);
            views.push_back(value);
            bin.insert(bin.end(), bytes.begin(), bytes.end());
            return uint32_t(views.size() - 1);
        }
        uint32_t accessor(const std::vector<uint8_t>& bytes,
                          const std::string&          type,
                          uint32_t                    component,
                          uint32_t                    count      = 3,
                          bool                        normalized = false,
                          uint32_t                    stride     = 0)
        {
            Json value                    = parse("{}");
            field(value, "bufferView")    = view(bytes, stride);
            field(value, "componentType") = component;
            field(value, "count")         = count;
            field(value, "type")          = type;
            if (normalized)
                field(value, "normalized") = true;
            auto& accessors = std::get<Array>(field(root, "accessors").value);
            accessors.push_back(value);
            return uint32_t(accessors.size() - 1);
        }
        void stream(const std::string&          name,
                    const std::vector<uint8_t>& bytes,
                    const std::string&          type,
                    uint32_t                    component,
                    bool                        normalized = false,
                    uint32_t                    stride     = 0)
        {
            const auto index          = accessor(bytes, type, component, 3, normalized, stride);
            field(attributes(), name) = index;
        }
        void uv(const std::string& name = "TEXCOORD_0")
        {
            stream(name, floats({0, 0.25f, 1, 0, 0, 1}), "VEC2", 5126);
        }
        void material()
        {
            uv();
            uv("TEXCOORD_1");
            field(root, "images")             = parse(R"([{"name":"image","uri":"data:image/png;base64,AQIDBA=="}])");
            field(root, "textures")           = parse(R"([{"source":0}])");
            field(root, "materials")          = parse(R"([{"name":"surface","pbrMetallicRoughness":{
            "baseColorFactor":[0.2,0.4,0.6,0.8],"metallicFactor":0.3,"roughnessFactor":0.7,
            "baseColorTexture":{"index":0,"texCoord":1},"metallicRoughnessTexture":{"index":0}},
            "normalTexture":{"index":0,"scale":0.6},"occlusionTexture":{"index":0,"strength":0.4},
            "emissiveTexture":{"index":0},"emissiveFactor":[0.1,0.2,0.3],
            "extensions":{"KHR_materials_emissive_strength":{"emissiveStrength":4}},
            "alphaMode":"MASK","alphaCutoff":0.2,"doubleSided":true}])");
            field(primitive(), "material")    = 0u;
            field(root, "extensionsRequired") = parse(R"(["KHR_materials_emissive_strength"])");
        }
        std::filesystem::path save(const std::filesystem::path& directory, const std::string& format = "external")
        {
            Json& buffer                = at(field(root, "buffers"), 0);
            field(buffer, "byteLength") = uint32_t(bin.size());
            if (format == "glb")
                erase(buffer, "uri");
            else if (format == "data")
                field(buffer, "uri") = "data:application/octet-stream;base64," + base64(bin);
            else
            {
                field(buffer, "uri") = "buffer%20data.bin";
                write(directory / "buffer data.bin", bin);
            }
            std::string text = GtsJsonParser::serialize(root);
            const auto  path = directory / (format == "glb" ? "fixture.glb" : "fixture.gltf");
            if (format != "glb")
            {
                write(path, {text.begin(), text.end()});
                return path;
            }
            while (text.size() % 4)
                text += ' ';
            auto padded = bin;
            while (padded.size() % 4)
                padded.push_back(0);
            std::vector<uint8_t> bytes;
            append32(bytes, 0x46546c67);
            append32(bytes, 2);
            append32(bytes, uint32_t(28 + text.size() + padded.size()));
            append32(bytes, uint32_t(text.size()));
            append32(bytes, 0x4e4f534a);
            bytes.insert(bytes.end(), text.begin(), text.end());
            append32(bytes, uint32_t(padded.size()));
            append32(bytes, 0x004e4942);
            bytes.insert(bytes.end(), padded.begin(), padded.end());
            write(path, bytes);
            return path;
        }
    };
    const GtsVertexAttribute* find(const GtsModelPrimitive& primitive, GtsVertexSemantic kind, uint32_t set = 0)
    {
        for (const auto& attribute : primitive.attributes)
            if (attribute.semantic == kind && attribute.setIndex == set)
                return &attribute;
        return nullptr;
    }
    bool has(const GtsModelImportResult& result, const std::string& code)
    {
        for (const auto& diagnostic : result.diagnostics())
            if (diagnostic.code == code)
                return true;
        return false;
    }
    GtsModelImportResult
    load(Fixture& fixture, const std::filesystem::path& root, const std::string& format = "external")
    {
        const auto path = fixture.save(root, format);
        return GtsGltfModelImporter{}.importAsset({path});
    }
    void good(const GtsModelImportResult& result)
    {
        std::string message;
        for (const auto& diagnostic : result.diagnostics())
            message += diagnostic.code + ": " + diagnostic.message + " @ " + diagnostic.location + "\n";
        require(result.succeeded(), message);
    }
    void bad(Fixture& fixture, const std::filesystem::path& root, const std::string& code)
    {
        const auto result = load(fixture, root);
        require(!result.succeeded() && !result.asset() && has(result, code), "Expected failure " + code);
    }
} // namespace

int main()
{
    try
    {
        const auto root = std::filesystem::temp_directory_path() / "gravitas_canonical_gltf_test";
        std::filesystem::create_directories(root);
        for (const auto format : {"external", "data", "glb"})
        {
            Fixture    f;
            const auto result = load(f, root, format);
            good(result);
            const auto& mesh      = result.asset()->meshes[0];
            const auto& primitive = mesh.primitives[0];
            require(mesh.name == "triangle" && primitive.indices == std::vector<uint32_t>({0, 1, 2}),
                    "minimal geometry");
            require(std::get<std::vector<glm::vec3>>(find(primitive, GtsVertexSemantic::Position)->values)[1] ==
                        glm::vec3(1, 0, 0),
                    "positions");
            require(!find(primitive, GtsVertexSemantic::Normal) && !find(primitive, GtsVertexSemantic::Tangent) &&
                        !find(primitive, GtsVertexSemantic::Color),
                    "absent streams stay absent");
        }
        for (uint32_t component : {5121, 5123, 5125})
        {
            Fixture              f;
            std::vector<uint8_t> indices;
            for (uint32_t index : {2, 1, 0})
                for (size_t byte = 0; byte < (component == 5121 ? 1 : component == 5123 ? 2 : 4); ++byte)
                    indices.push_back(uint8_t(index >> (byte * 8)));
            field(f.primitive(), "indices") = f.accessor(indices, "SCALAR", component);
            auto result                     = load(f, root, "glb");
            good(result);
            require(result.asset()->meshes[0].primitives[0].indices == std::vector<uint32_t>({2, 1, 0}),
                    "index encodings");
        }
        {
            Fixture f;
            f.stream("NORMAL", floats({0, 0, 1, 0, 0, 1, 0, 0, 1}), "VEC3", 5126);
            f.stream("TANGENT", floats({1, 0, 0, 1, 1, 0, 0, -1, 1, 0, 0, 1}), "VEC4", 5126);
            f.uv();
            f.uv("TEXCOORD_1");
            f.uv("TEXCOORD_2");
            f.stream("COLOR_0", {255, 128, 0, 255, 0, 255, 128, 255, 0, 0, 255, 255}, "VEC4", 5121, true);
            f.stream("COLOR_1", {255, 0, 128, 0, 0, 255, 128, 0, 0, 0, 255, 0}, "VEC3", 5121, true, 4);
            for (const auto set : {"0", "1"})
            {
                f.stream(std::string("JOINTS_") + set, {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11}, "VEC4", 5121);
                f.stream(std::string("WEIGHTS_") + set, {255, 0, 0, 0, 255, 0, 0, 0, 255, 0, 0, 0}, "VEC4", 5121, true);
            }
            const auto result = load(f, root);
            good(result);
            const auto& p = result.asset()->meshes[0].primitives[0];
            require(std::get<std::vector<glm::vec4>>(find(p, GtsVertexSemantic::Tangent)->values)[0].w == -1,
                    "tangent handedness");
            require(std::get<std::vector<glm::vec2>>(find(p, GtsVertexSemantic::TexCoord, 1)->values)[0].y == 0.75f,
                    "UV1 V convention");
            require(std::get<std::vector<glm::vec4>>(find(p, GtsVertexSemantic::Color)->values)[0].g == 128.0f / 255,
                    "normalized color");
            require(std::get<std::vector<glm::vec4>>(find(p, GtsVertexSemantic::Color, 1)->values)[0].a == 1,
                    "RGB expands alpha");
            require(std::get<std::vector<glm::uvec4>>(find(p, GtsVertexSemantic::Joints, 1)->values)[2].w == 11,
                    "numbered joint streams");
            require(std::get<std::vector<glm::vec4>>(find(p, GtsVertexSemantic::Weights, 1)->values)[0].x == 1,
                    "numbered weights");
        }
        {
            Fixture f;
            f.bin                     = floats({0, 0, 0, 0, 0.25f, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1});
            auto& view                = at(field(f.root, "bufferViews"), 0);
            field(view, "byteLength") = 60u;
            field(view, "byteStride") = 20u;
            Json uv                   = at(field(f.root, "accessors"), 0);
            field(uv, "type")         = "VEC2";
            field(uv, "byteOffset")   = 12u;
            std::get<Array>(field(f.root, "accessors").value).push_back(uv);
            field(f.attributes(), "TEXCOORD_0") = 1u;
            const auto result                   = load(f, root);
            good(result);
            require(std::get<std::vector<glm::vec2>>(
                        find(result.asset()->meshes[0].primitives[0], GtsVertexSemantic::TexCoord)->values)[0]
                            .y == 0.75f,
                    "interleaved accessor");
        }
        {
            Fixture f;
            f.material();
            Json second = f.primitive();
            std::get<Array>(field(at(field(f.root, "meshes"), 0), "primitives").value).push_back(second);
            Json mesh           = at(field(f.root, "meshes"), 0);
            field(mesh, "name") = "second";
            std::get<Array>(field(f.root, "meshes").value).push_back(mesh);
            field(f.root, "nodes") = parse(
                R"([{"name":"excluded","mesh":0},{"name":"parent","children":[2],"translation":[1,2,3],"rotation":[0,0,1,0],"scale":[2,3,4]},{"name":"child","mesh":1,"matrix":[1,0,0,0,0,1,0,0,0,0,1,0,4,5,6,1]}])");
            field(f.root, "scenes") = parse(R"([{"nodes":[0]},{"nodes":[1]}])");
            field(f.root, "scene")  = 1u;
            auto result             = load(f, root, "glb");
            good(result);
            const auto& model = *result.asset();
            require(model.meshes.size() == 2 && model.meshes[0].primitives.size() == 2, "mesh primitive boundaries");
            require(model.nodes.size() == 2 && model.rootNodes == std::vector<uint32_t>({0}) &&
                        model.nodes[0].children == std::vector<uint32_t>({1}),
                    "selected scene remapping");
            require(model.nodes[1].meshIndex == 1 && model.nodes[1].localTransform[3] == glm::vec4(4, 5, 6, 1),
                    "matrix mesh reference");
            require(model.nodes[0].localTransform[3] == glm::vec4(1, 2, 3, 1) &&
                        model.nodes[0].localTransform[0][0] == -2,
                    "TRS order");
            const auto& m = model.materials[0];
            require(m.baseColor == glm::vec4(.2f, .4f, .6f, .8f) && m.metallic == .3f && m.roughness == .7f,
                    "PBR factors");
            require(m.normalScale == .6f && m.ambientOcclusionStrength == .4f && m.emissiveStrength == 4 &&
                        m.emissiveFactor == glm::vec3(.1f, .2f, .3f),
                    "appearance factors");
            require(m.metallicImage->image.imageIndex == m.roughnessImage->image.imageIndex &&
                        m.metallicImage->channel == GtsModelTextureChannel::Blue &&
                        m.roughnessImage->channel == GtsModelTextureChannel::Green,
                    "metal rough mapping");
            require(m.ambientOcclusionImage->channel == GtsModelTextureChannel::Red &&
                        m.baseColorImage->texCoordSet == 1,
                    "AO UV selection");
            require(m.alphaMode == GtsModelAlphaMode::Mask && m.alphaCutoff == .2f && m.doubleSided, "surface state");
            require(std::get<GtsModelEmbeddedImage>(model.images[0].source).bytes == std::vector<uint8_t>({1, 2, 3, 4}),
                    "data URI image encoded bytes");
            erase(f.root, "scene");
            result = load(f, root);
            good(result);
            require(result.asset()->nodes.size() == 1 && has(result, "GLTF_SCENE_DEFAULT"), "defaultless scene zero");
            erase(f.root, "scenes");
            result = load(f, root);
            good(result);
            require(result.asset()->nodes.size() == 3 && result.asset()->rootNodes.size() == 2 &&
                        has(result, "GLTF_SCENE_LIBRARY"),
                    "scene-free library");
        }
        {
            Fixture f;
            f.material();
            field(at(field(f.root, "images"), 0), "uri") = "external%20image.png";
            auto result                                  = load(f, root);
            good(result);
            require(std::get<std::filesystem::path>(result.asset()->images[0].source) == root / "external image.png" &&
                        has(result, "GLTF_IMAGE_MISSING"),
                    "external image identity");
            Json image = at(field(f.root, "images"), 0);
            std::get<Array>(field(f.root, "images").value).push_back(image);
            std::get<Array>(field(f.root, "textures").value).push_back(parse(R"({"source":1,"sampler":0})"));
            field(f.root, "samplers") = parse(R"([{"wrapS":33071}])");
            result                    = load(f, root);
            good(result);
            require(result.asset()->images.size() == 1 && has(result, "GLTF_SAMPLER_UNSUPPORTED"),
                    "dedup and sampler diagnostic");
            field(f.root, "images")                             = parse("[{}]");
            field(f.root, "textures")                           = parse(R"([{"source":0}])");
            const auto view                                     = f.view({1, 2, 3, 4});
            field(at(field(f.root, "images"), 0), "bufferView") = view;
            field(at(field(f.root, "images"), 0), "mimeType")   = "image/png";
            result                                              = load(f, root, "glb");
            good(result);
            require(std::get<GtsModelEmbeddedImage>(result.asset()->images[0].source).mimeType == "image/png",
                    "GLB image view");
        }
        {
            Fixture f;
            auto    result = load(f, root);
            good(result);
            const auto materialIndex = result.asset()->meshes[0].primitives[0].materialIndex;
            require(materialIndex && result.asset()->materials[*materialIndex].metallic == 1,
                    "implicit glTF material is resolved above the format wall");
            erase(f.root, "nodes");
            erase(f.root, "scenes");
            erase(f.root, "scene");
            result = load(f, root);
            good(result);
            require(result.asset()->nodes.empty() && result.asset()->meshes.size() == 1, "mesh-only library");
        }
        {
            Fixture f;
            f.stream("TEXCOORD_0", {0, 0, 255, 255, 255, 255, 0, 0, 0, 0, 0, 0}, "VEC2", 5123, true);
            std::vector<uint8_t> joints;
            for (int i = 0; i < 3; ++i)
                joints.insert(joints.end(), {0xfe, 0xff, 0, 0, 0, 0, 0, 0});
            f.stream("JOINTS_0", joints, "VEC4", 5123);
            f.stream("WEIGHTS_0", floats({1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0}), "VEC4", 5126);
            const auto result = load(f, root);
            good(result);
            const auto& primitive = result.asset()->meshes[0].primitives[0];
            require(std::get<std::vector<glm::uvec4>>(find(primitive, GtsVertexSemantic::Joints)->values)[0].x == 65534,
                    "ushort joints stay integers");
            require(std::get<std::vector<glm::vec2>>(find(primitive, GtsVertexSemantic::TexCoord)->values)[0].y == 0,
                    "normalized ushort UVs");
        }
        {
            Fixture f;
            f.material();
            field(at(field(f.root, "materials"), 0), "alphaMode") = "BLEND";
            auto result                                           = load(f, root);
            good(result);
            require(result.asset()->materials[0].alphaMode == GtsModelAlphaMode::Blend, "blend material");
            field(at(field(f.root, "materials"), 0), "alphaMode") = "invalid";
            bad(f, root, "GLTF_ALPHA_MODE");
        }
        {
            Fixture f;
            field(f.primitive(), "indices") = f.accessor({0, 1, 2}, "SCALAR", 5121, 3, true);
            bad(f, root, "GLTF_INDEX_TYPE");
        }
        {
            Fixture f;
            field(at(field(f.root, "nodes"), 0), "matrix")      = parse("[1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1]");
            field(at(field(f.root, "nodes"), 0), "translation") = parse("[0,0,0]");
            bad(f, root, "GLTF_NODE_TRANSFORM");
        }
        {
            Fixture f;
            f.stream("NORMAL", floats({0, 0, 1, 0, 0, 1, 0, 0, 1}), "VEC3", 5126);
            field(at(field(f.root, "bufferViews"), 1), "byteLength") = 24u;
            bad(f, root, "GLTF_ACCESSOR_RANGE");
        }
        {
            Fixture f;
            field(f.attributes(), "_CUSTOM") = 0u;
            bad(f, root, "GLTF_ATTRIBUTE_UNSUPPORTED");
        }
        {
            Fixture f;
            erase(at(field(f.root, "accessors"), 0), "bufferView");
            bad(f, root, "GLTF_ACCESSOR_SOURCE_UNSUPPORTED");
        }
        // Decoder and source-shape failures must never expose a partial canonical asset.
        {
            Fixture f;
            field(at(field(f.root, "accessors"), 0), "count") = UINT32_MAX;
            bad(f, root, "GLTF_ACCESSOR_RANGE");
        }
        {
            Fixture f;
            field(at(field(f.root, "bufferViews"), 0), "byteLength") = 35u;
            bad(f, root, "GLTF_ACCESSOR_RANGE");
        }
        {
            Fixture f;
            field(at(field(f.root, "bufferViews"), 0), "byteLength") = UINT32_MAX;
            bad(f, root, "GLTF_VIEW_RANGE");
        }
        {
            Fixture f;
            field(at(field(f.root, "accessors"), 0), "byteOffset") = UINT32_MAX;
            bad(f, root, "GLTF_ACCESSOR_RANGE");
        }
        {
            Fixture f;
            field(at(field(f.root, "accessors"), 0), "sparse") = parse("{}");
            bad(f, root, "GLTF_SPARSE_UNSUPPORTED");
        }
        {
            Fixture f;
            field(f.primitive(), "indices") = f.accessor(floats({0, 1, 2}), "SCALAR", 5126);
            bad(f, root, "GLTF_INDEX_TYPE");
        }
        {
            Fixture f;
            field(f.primitive(), "indices") = f.accessor({0, 1, 1, 2, 2, 0}, "VEC2", 5121);
            bad(f, root, "GLTF_INDEX_TYPE");
        }
        {
            Fixture              f;
            std::vector<uint8_t> bytes;
            for (uint32_t i : {0u, 1u, 16777217u})
                append32(bytes, i);
            field(f.primitive(), "indices") = f.accessor(bytes, "SCALAR", 5125);
            bad(f, root, "GLTF_INDEX_RANGE");
        }
        {
            Fixture f;
            field(f.attributes(), "NORMAL") = 999u;
            bad(f, root, "GLTF_ACCESSOR_REFERENCE");
        }
        {
            Fixture f;
            f.stream("COLOR_0", floats({0, 1, 0, 1, 0, 1}), "VEC2", 5126);
            bad(f, root, "GLTF_ATTRIBUTE_TYPE");
        }
        {
            Fixture f;
            f.stream("NORMAL", floats({0, 0, 1, 0, 0, 1, 0, 0, 1}), "VEC3", 5126);
            field(at(field(f.root, "accessors"), 1), "count") = 2u;
            bad(f, root, "GLTF_ATTRIBUTE_COUNT");
        }
        {
            Fixture f;
            f.uv("TEXCOORD_1");
            bad(f, root, "GLTF_ATTRIBUTE_SETS");
        }
        {
            Fixture f;
            f.stream("JOINTS_0", std::vector<uint8_t>(12), "VEC4", 5121);
            bad(f, root, "GLTF_INFLUENCE_PAIR");
        }
        {
            Fixture f;
            field(f.primitive(), "mode") = 5u;
            bad(f, root, "GLTF_TOPOLOGY_UNSUPPORTED");
        }
        {
            Fixture f;
            field(f.primitive(), "targets") = parse("[{}]");
            bad(f, root, "GLTF_MORPH_UNSUPPORTED");
        }
        {
            Fixture f;
            field(f.root, "animations") = parse("[{}]");
            bad(f, root, "GLTF_ANIMATION_UNSUPPORTED");
        }
        {
            Fixture f;
            field(f.root, "skins")                       = parse("[{}]");
            field(at(field(f.root, "nodes"), 0), "skin") = 0u;
            bad(f, root, "GLTF_SKIN_BINDING_UNSUPPORTED");
        }
        {
            Fixture f;
            field(at(field(f.root, "nodes"), 0), "children") = parse("[4]");
            bad(f, root, "GLTF_CHILD_REFERENCE");
        }
        {
            Fixture f;
            field(f.root, "nodes") = parse(R"([{"children":[1,1]},{}])");
            bad(f, root, "GLTF_MULTIPLE_PARENT");
        }
        {
            Fixture f;
            field(f.root, "nodes") = parse(R"([{"children":[2]},{"children":[2]},{}])");
            bad(f, root, "GLTF_MULTIPLE_PARENT");
        }
        {
            Fixture f;
            field(f.root, "nodes") = parse(R"([{"children":[1]},{"children":[0]}])");
            bad(f, root, "GLTF_NODE_CYCLE");
        }
        {
            Fixture f;
            field(at(field(f.root, "nodes"), 0), "mesh") = 10u;
            bad(f, root, "GLTF_MESH_REFERENCE");
        }
        {
            Fixture f;
            field(at(field(f.root, "meshes"), 0), "primitives") = parse("[]");
            bad(f, root, "GLTF_MESH_EMPTY");
        }
        {
            Fixture f;
            field(f.root, "scene") = 3u;
            bad(f, root, "GLTF_SCENE_REFERENCE");
        }
        {
            Fixture f;
            field(at(field(f.root, "scenes"), 0), "nodes") = parse("[0,0]");
            bad(f, root, "GLTF_SCENE_ROOT");
        }
        {
            Fixture f;
            field(f.root, "extensionsRequired") = parse(R"(["KHR_draco_mesh_compression"])");
            bad(f, root, "GLTF_REQUIRED_EXTENSION");
        }
        {
            Fixture f;
            field(f.root, "extensionsUsed") = parse(R"(["TEST_unknown"])");
            auto result                     = load(f, root);
            good(result);
            require(has(result, "GLTF_OPTIONAL_EXTENSION"), "optional extension warning");
        }
        {
            Fixture f;
            field(f.primitive(), "material") = 5u;
            auto result                      = load(f, root);
            require(!result.succeeded() && !result.asset(), "canonical validation gates invalid material");
        }
        {
            Fixture f;
            f.material();
            erase(f.attributes(), "TEXCOORD_1");
            auto result = load(f, root);
            require(!result.succeeded(), "canonical material UV validation");
        }
        {
            for (int change = 0; change < 4; ++change)
            {
                Fixture              f;
                const auto           path = f.save(root, "glb");
                std::ifstream        file(path, std::ios::binary);
                std::vector<uint8_t> bytes{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
                file.close();
                if (change == 0)
                    bytes[8] ^= 4; // total length
                if (change == 1)
                    bytes[16] = 0; // first chunk type
                if (change == 2)
                    bytes[12] |= 1; // chunk alignment
                if (change == 3)
                {
                    bytes.push_back(0);
                    const auto size = uint32_t(bytes.size());
                    for (int i = 0; i < 4; ++i)
                        bytes[8 + i] = uint8_t(size >> (i * 8));
                }
                write(path, bytes);
                const auto result = GtsGltfModelImporter{}.importAsset({path});
                require(!result.succeeded(), "malformed GLB structure");
            }
        }
        {
            Fixture f;
            f.uv();
            const auto a = load(f, root, "external"), b = load(f, root, "glb");
            good(a);
            good(b);
            const auto& pa = a.asset()->meshes[0].primitives[0];
            const auto& pb = b.asset()->meshes[0].primitives[0];
            require(pa.indices == pb.indices && pa.attributes.size() == pb.attributes.size(),
                    "equivalent canonical geometry");
            for (size_t i = 0; i < pa.attributes.size(); ++i)
                require(pa.attributes[i].values == pb.attributes[i].values, "deterministic attribute values");
            require(a.asset()->rootNodes == b.asset()->rootNodes &&
                        a.asset()->nodes[0].localTransform == b.asset()->nodes[0].localTransform,
                    "deterministic nodes");
        }
        std::filesystem::remove_all(root);
        std::puts("GtsGltfModelImporterTest passed");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "FAIL: %s\n", error.what());
        return 1;
    }
}
