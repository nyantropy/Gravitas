#pragma once

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "GtsGltfModelImporter.h"
#include "GtsJsonParser.h"
#include "GtsModelImportResult.h"

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
    void writeBytes(const std::filesystem::path& path, const std::vector<uint8_t>& bytes)
    {
        std::ofstream file(path, std::ios::binary);
        file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        require(file.good(), "fixture write");
    }
    enum class GltfFixtureFormat
    {
        External,
        DataUri,
        Glb
    };

    // builds deterministic synthetic glTF/GLB source assets for importer tests
    struct GltfFixtureBuilder
    {
        Json root = parse(R"({"asset":{"version":"2.0"},"buffers":[{}],"bufferViews":[],"accessors":[],
        "meshes":[{"name":"triangle","primitives":[{"attributes":{}}]}],
        "nodes":[{"name":"root","mesh":0}],"scenes":[{"nodes":[0]}],"scene":0})");
        std::vector<uint8_t> bin;
        GltfFixtureBuilder()
        {
            addVertexStream("POSITION", floats({0, 0, 0, 1, 0, 0, 0, 1, 0}), "VEC3", 5126);
        }
        Json& primitive()
        {
            return at(field(at(field(root, "meshes"), 0), "primitives"), 0);
        }
        Json& attributes()
        {
            return field(primitive(), "attributes");
        }
        uint32_t addBufferView(const std::vector<uint8_t>& bytes, uint32_t stride = 0)
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
        uint32_t addAccessor(const std::vector<uint8_t>& bytes,
                             const std::string&          type,
                             uint32_t                    component,
                             uint32_t                    count      = 3,
                             bool                        normalized = false,
                             uint32_t                    stride     = 0)
        {
            Json value                    = parse("{}");
            field(value, "bufferView")    = addBufferView(bytes, stride);
            field(value, "componentType") = component;
            field(value, "count")         = count;
            field(value, "type")          = type;
            if (normalized)
                field(value, "normalized") = true;
            auto& accessors = std::get<Array>(field(root, "accessors").value);
            accessors.push_back(value);
            return uint32_t(accessors.size() - 1);
        }
        void addVertexStream(const std::string&          name,
                             const std::vector<uint8_t>& bytes,
                             const std::string&          type,
                             uint32_t                    component,
                             bool                        normalized = false,
                             uint32_t                    stride     = 0)
        {
            const auto index          = addAccessor(bytes, type, component, 3, normalized, stride);
            field(attributes(), name) = index;
        }
        void addTexCoordStream(const std::string& name = "TEXCOORD_0")
        {
            addVertexStream(name, floats({0, 0.25f, 1, 0, 0, 1}), "VEC2", 5126);
        }
        uint32_t addAnimation(const std::string& name = {})
        {
            if (!root.find("animations"))
                field(root, "animations") = Array{};
            Json animation = parse(R"({"samplers":[],"channels":[]})");
            if (!name.empty())
                field(animation, "name") = name;
            auto& animations = std::get<Array>(field(root, "animations").value);
            animations.push_back(std::move(animation));
            return static_cast<uint32_t>(animations.size() - 1);
        }
        uint32_t addAnimationTimes(std::initializer_list<float> times)
        {
            require(times.size() > 0, "Animation fixture needs key times");
            const auto index       = addAccessor(floats(times), "SCALAR", 5126, static_cast<uint32_t>(times.size()));
            auto&      accessor    = at(field(root, "accessors"), index);
            field(accessor, "min") = Array{Json(*std::min_element(times.begin(), times.end()))};
            field(accessor, "max") = Array{Json(*std::max_element(times.begin(), times.end()))};
            return index;
        }
        uint32_t addAnimationSampler(uint32_t           animation,
                                     uint32_t           input,
                                     uint32_t           output,
                                     const std::string& interpolation = "LINEAR")
        {
            Json sampler                    = parse("{}");
            field(sampler, "input")         = input;
            field(sampler, "output")        = output;
            field(sampler, "interpolation") = interpolation;
            auto& samplers = std::get<Array>(field(at(field(root, "animations"), animation), "samplers").value);
            samplers.push_back(std::move(sampler));
            return static_cast<uint32_t>(samplers.size() - 1);
        }
        void addAnimationChannel(uint32_t animation, uint32_t sampler, uint32_t node, const std::string& path)
        {
            Json channel                            = parse(R"({"target":{}})");
            field(channel, "sampler")               = sampler;
            field(field(channel, "target"), "node") = node;
            field(field(channel, "target"), "path") = path;
            std::get<Array>(field(at(field(root, "animations"), animation), "channels").value)
                .push_back(std::move(channel));
        }
        void setTexturedMaterial()
        {
            addTexCoordStream();
            addTexCoordStream("TEXCOORD_1");
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
        std::filesystem::path write(const std::filesystem::path& directory,
                                    GltfFixtureFormat            format = GltfFixtureFormat::External)
        {
            Json& buffer                = at(field(root, "buffers"), 0);
            field(buffer, "byteLength") = uint32_t(bin.size());
            if (format == GltfFixtureFormat::Glb)
                erase(buffer, "uri");
            else if (format == GltfFixtureFormat::DataUri)
                field(buffer, "uri") = "data:application/octet-stream;base64," + base64(bin);
            else
            {
                field(buffer, "uri") = "buffer%20data.bin";
                writeBytes(directory / "buffer data.bin", bin);
            }
            std::string text = GtsJsonParser::serialize(root);
            const auto  path = directory / (format == GltfFixtureFormat::Glb ? "fixture.glb" : "fixture.gltf");
            if (format != GltfFixtureFormat::Glb)
            {
                writeBytes(path, {text.begin(), text.end()});
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
            writeBytes(path, bytes);
            return path;
        }
    };
    const GtsVertexAttribute*
    findVertexAttribute(const GtsModelPrimitive& primitive, GtsVertexSemantic kind, uint32_t set = 0)
    {
        for (const auto& attribute : primitive.attributes)
            if (attribute.semantic == kind && attribute.setIndex == set)
                return &attribute;
        return nullptr;
    }
    bool hasDiagnostic(const GtsModelImportResult& result, const std::string& code)
    {
        for (const auto& diagnostic : result.diagnostics())
            if (diagnostic.code == code)
                return true;
        return false;
    }
    GtsModelImportResult importFixture(GltfFixtureBuilder&          fixture,
                                       const std::filesystem::path& root,
                                       GltfFixtureFormat            format = GltfFixtureFormat::External)
    {
        const auto path = fixture.write(root, format);
        return GtsGltfModelImporter{}.importAsset({path});
    }
    void requireStaticImportSuccess(const GtsModelImportResult& result)
    {
        std::string message;
        for (const auto& diagnostic : result.diagnostics())
            message += diagnostic.code + ": " + diagnostic.message + " @ " + diagnostic.location + "\n";
        require(result.succeeded(), message);
        require(result.bundle() && result.bundle()->model && result.bundle()->skeletons.empty() &&
                    result.bundle()->animationClips.empty(),
                "Static glTF produces a primary model without skeleton or animation products");
    }
    void requireImportFailure(GltfFixtureBuilder& fixture, const std::filesystem::path& root, const std::string& code)
    {
        const auto result = importFixture(fixture, root);
        require(!result.succeeded() && !result.asset() && !result.bundle() && hasDiagnostic(result, code),
                "Expected failure " + code);
    }
} // namespace
