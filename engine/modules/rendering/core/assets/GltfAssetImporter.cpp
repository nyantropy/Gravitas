#include "GltfAssetImporter.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>
#include <fstream>
#include <limits>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <utility>

#include <gtc/quaternion.hpp>
#include <gtx/quaternion.hpp>

#include "MeshGeometryProcessor.h"

namespace gts::rendering
{
namespace
{
    struct JsonValue
    {
        enum class Type
        {
            Null,
            Bool,
            Number,
            String,
            Array,
            Object
        };

        Type type = Type::Null;
        bool boolean = false;
        double number = 0.0;
        std::string string;
        std::vector<JsonValue> array;
        std::map<std::string, JsonValue> object;

        bool isObject() const { return type == Type::Object; }
        bool isArray() const { return type == Type::Array; }
        bool isString() const { return type == Type::String; }
        bool isNumber() const { return type == Type::Number; }
        bool isBool() const { return type == Type::Bool; }
    };

    class JsonParser
    {
    public:
        explicit JsonParser(std::string_view text)
            : text(text)
        {
        }

        bool parse(JsonValue& out, std::string& error)
        {
            skipWhitespace();
            if (!parseValue(out, error))
                return false;
            skipWhitespace();
            if (cursor != text.size())
            {
                error = "Unexpected trailing JSON data";
                return false;
            }
            return true;
        }

    private:
        bool parseValue(JsonValue& out, std::string& error)
        {
            skipWhitespace();
            if (cursor >= text.size())
            {
                error = "Unexpected end of JSON";
                return false;
            }

            const char ch = text[cursor];
            if (ch == '{')
                return parseObject(out, error);
            if (ch == '[')
                return parseArray(out, error);
            if (ch == '"')
                return parseStringValue(out, error);
            if (ch == '-' || std::isdigit(static_cast<unsigned char>(ch)) != 0)
                return parseNumber(out, error);
            if (consumeLiteral("true"))
            {
                out.type = JsonValue::Type::Bool;
                out.boolean = true;
                return true;
            }
            if (consumeLiteral("false"))
            {
                out.type = JsonValue::Type::Bool;
                out.boolean = false;
                return true;
            }
            if (consumeLiteral("null"))
            {
                out.type = JsonValue::Type::Null;
                return true;
            }

            error = "Unexpected JSON token";
            return false;
        }

        bool parseObject(JsonValue& out, std::string& error)
        {
            out = {};
            out.type = JsonValue::Type::Object;
            cursor += 1;
            skipWhitespace();
            if (consume('}'))
                return true;

            while (cursor < text.size())
            {
                JsonValue key;
                if (!parseStringValue(key, error))
                    return false;
                skipWhitespace();
                if (!consume(':'))
                {
                    error = "Expected ':' in JSON object";
                    return false;
                }
                JsonValue value;
                if (!parseValue(value, error))
                    return false;
                out.object.emplace(std::move(key.string), std::move(value));
                skipWhitespace();
                if (consume('}'))
                    return true;
                if (!consume(','))
                {
                    error = "Expected ',' in JSON object";
                    return false;
                }
                skipWhitespace();
            }

            error = "Unterminated JSON object";
            return false;
        }

        bool parseArray(JsonValue& out, std::string& error)
        {
            out = {};
            out.type = JsonValue::Type::Array;
            cursor += 1;
            skipWhitespace();
            if (consume(']'))
                return true;

            while (cursor < text.size())
            {
                JsonValue value;
                if (!parseValue(value, error))
                    return false;
                out.array.push_back(std::move(value));
                skipWhitespace();
                if (consume(']'))
                    return true;
                if (!consume(','))
                {
                    error = "Expected ',' in JSON array";
                    return false;
                }
                skipWhitespace();
            }

            error = "Unterminated JSON array";
            return false;
        }

        bool parseStringValue(JsonValue& out, std::string& error)
        {
            if (!consume('"'))
            {
                error = "Expected JSON string";
                return false;
            }

            out = {};
            out.type = JsonValue::Type::String;
            while (cursor < text.size())
            {
                const char ch = text[cursor++];
                if (ch == '"')
                    return true;
                if (ch != '\\')
                {
                    out.string.push_back(ch);
                    continue;
                }
                if (cursor >= text.size())
                {
                    error = "Unterminated JSON string escape";
                    return false;
                }
                const char escaped = text[cursor++];
                switch (escaped)
                {
                    case '"': out.string.push_back('"'); break;
                    case '\\': out.string.push_back('\\'); break;
                    case '/': out.string.push_back('/'); break;
                    case 'b': out.string.push_back('\b'); break;
                    case 'f': out.string.push_back('\f'); break;
                    case 'n': out.string.push_back('\n'); break;
                    case 'r': out.string.push_back('\r'); break;
                    case 't': out.string.push_back('\t'); break;
                    case 'u':
                        if (cursor + 4u > text.size())
                        {
                            error = "Truncated JSON unicode escape";
                            return false;
                        }
                        cursor += 4u;
                        out.string.push_back('?');
                        break;
                    default:
                        error = "Unsupported JSON string escape";
                        return false;
                }
            }

            error = "Unterminated JSON string";
            return false;
        }

        bool parseNumber(JsonValue& out, std::string& error)
        {
            const size_t begin = cursor;
            if (text[cursor] == '-')
                cursor += 1;
            while (cursor < text.size() && std::isdigit(static_cast<unsigned char>(text[cursor])) != 0)
                cursor += 1;
            if (cursor < text.size() && text[cursor] == '.')
            {
                cursor += 1;
                while (cursor < text.size() && std::isdigit(static_cast<unsigned char>(text[cursor])) != 0)
                    cursor += 1;
            }
            if (cursor < text.size() && (text[cursor] == 'e' || text[cursor] == 'E'))
            {
                cursor += 1;
                if (cursor < text.size() && (text[cursor] == '+' || text[cursor] == '-'))
                    cursor += 1;
                while (cursor < text.size() && std::isdigit(static_cast<unsigned char>(text[cursor])) != 0)
                    cursor += 1;
            }

            try
            {
                out = {};
                out.type = JsonValue::Type::Number;
                out.number = std::stod(std::string(text.substr(begin, cursor - begin)));
                return true;
            }
            catch (const std::exception&)
            {
                error = "Invalid JSON number";
                return false;
            }
        }

        bool consume(char ch)
        {
            if (cursor >= text.size() || text[cursor] != ch)
                return false;
            cursor += 1;
            return true;
        }

        bool consumeLiteral(std::string_view literal)
        {
            if (text.substr(cursor, literal.size()) != literal)
                return false;
            cursor += literal.size();
            return true;
        }

        void skipWhitespace()
        {
            while (cursor < text.size() &&
                   std::isspace(static_cast<unsigned char>(text[cursor])) != 0)
            {
                cursor += 1;
            }
        }

        std::string_view text;
        size_t cursor = 0;
    };

    const JsonValue* member(const JsonValue& value, const std::string& key)
    {
        if (!value.isObject())
            return nullptr;
        auto it = value.object.find(key);
        return it == value.object.end() ? nullptr : &it->second;
    }

    const JsonValue* at(const JsonValue& value, size_t index)
    {
        if (!value.isArray() || index >= value.array.size())
            return nullptr;
        return &value.array[index];
    }

    int32_t intValue(const JsonValue* value, int32_t fallback = 0)
    {
        if (value == nullptr || !value->isNumber())
            return fallback;
        return static_cast<int32_t>(value->number);
    }

    uint32_t uintValue(const JsonValue* value, uint32_t fallback = 0)
    {
        if (value == nullptr || !value->isNumber() || value->number < 0.0)
            return fallback;
        return static_cast<uint32_t>(value->number);
    }

    float floatValue(const JsonValue* value, float fallback = 0.0f)
    {
        if (value == nullptr || !value->isNumber())
            return fallback;
        return static_cast<float>(value->number);
    }

    bool boolValue(const JsonValue* value, bool fallback = false)
    {
        if (value == nullptr || !value->isBool())
            return fallback;
        return value->boolean;
    }

    std::string stringValue(const JsonValue* value, std::string fallback = {})
    {
        if (value == nullptr || !value->isString())
            return fallback;
        return value->string;
    }

    glm::vec3 vec3Value(const JsonValue* value, glm::vec3 fallback = {})
    {
        if (value == nullptr || !value->isArray() || value->array.size() < 3u)
            return fallback;
        return {
            floatValue(&value->array[0], fallback.x),
            floatValue(&value->array[1], fallback.y),
            floatValue(&value->array[2], fallback.z)
        };
    }

    glm::vec4 vec4Value(const JsonValue* value, glm::vec4 fallback = {})
    {
        if (value == nullptr || !value->isArray() || value->array.size() < 4u)
            return fallback;
        return {
            floatValue(&value->array[0], fallback.x),
            floatValue(&value->array[1], fallback.y),
            floatValue(&value->array[2], fallback.z),
            floatValue(&value->array[3], fallback.w)
        };
    }

    void addDiagnostic(AssetImportResult& result,
                       AssetDiagnosticSeverity severity,
                       std::string code,
                       std::string message,
                       const std::filesystem::path& sourcePath)
    {
        result.diagnostics.push_back({
            severity,
            std::move(code),
            std::move(message),
            sourcePath,
            0
        });
    }

    void addDependency(AssetImportResult& result,
                       std::filesystem::path path,
                       AssetDependencyType type,
                       bool required = true)
    {
        path = path.lexically_normal();
        for (const AssetDependency& dependency : result.dependencies)
        {
            if (dependency.type == type && dependency.path == path)
                return;
        }
        result.dependencies.push_back({std::move(path), type, required});
    }

    bool readFileBytes(const std::filesystem::path& path,
                       std::vector<uint8_t>& bytes)
    {
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file)
            return false;
        const std::streamsize size = file.tellg();
        if (size < 0)
            return false;
        file.seekg(0, std::ios::beg);
        bytes.resize(static_cast<size_t>(size));
        return bytes.empty() || static_cast<bool>(file.read(reinterpret_cast<char*>(bytes.data()), size));
    }

    std::string readFileText(const std::filesystem::path& path)
    {
        std::ifstream file(path, std::ios::binary);
        std::ostringstream stream;
        stream << file.rdbuf();
        return stream.str();
    }

    uint32_t readU32LE(const std::vector<uint8_t>& bytes, size_t offset)
    {
        if (offset + 4u > bytes.size())
            return 0;
        return static_cast<uint32_t>(bytes[offset + 0u])
            | (static_cast<uint32_t>(bytes[offset + 1u]) << 8u)
            | (static_cast<uint32_t>(bytes[offset + 2u]) << 16u)
            | (static_cast<uint32_t>(bytes[offset + 3u]) << 24u);
    }

    int base64Value(char ch)
    {
        if (ch >= 'A' && ch <= 'Z')
            return ch - 'A';
        if (ch >= 'a' && ch <= 'z')
            return ch - 'a' + 26;
        if (ch >= '0' && ch <= '9')
            return ch - '0' + 52;
        if (ch == '+')
            return 62;
        if (ch == '/')
            return 63;
        return -1;
    }

    bool decodeBase64(std::string_view encoded, std::vector<uint8_t>& bytes)
    {
        bytes.clear();
        uint32_t accumulator = 0;
        int bits = 0;
        for (char ch : encoded)
        {
            if (std::isspace(static_cast<unsigned char>(ch)) != 0)
                continue;
            if (ch == '=')
                break;
            const int value = base64Value(ch);
            if (value < 0)
                return false;
            accumulator = (accumulator << 6u) | static_cast<uint32_t>(value);
            bits += 6;
            if (bits >= 8)
            {
                bits -= 8;
                bytes.push_back(static_cast<uint8_t>((accumulator >> bits) & 0xFFu));
            }
        }
        return true;
    }

    bool decodeDataUri(const std::string& uri,
                       std::string& mimeType,
                       std::vector<uint8_t>& bytes)
    {
        if (!uri.starts_with("data:"))
            return false;
        const size_t comma = uri.find(',');
        if (comma == std::string::npos)
            return false;
        const std::string metadata = uri.substr(5u, comma - 5u);
        const bool base64 = metadata.find(";base64") != std::string::npos;
        const size_t semicolon = metadata.find(';');
        mimeType = metadata.substr(0u, semicolon == std::string::npos ? metadata.size() : semicolon);
        if (!base64)
            return false;
        return decodeBase64(std::string_view(uri).substr(comma + 1u), bytes);
    }

    std::string extensionForMimeType(const std::string& mimeType)
    {
        if (mimeType == "image/jpeg")
            return ".jpg";
        if (mimeType == "image/png")
            return ".png";
        if (mimeType == "image/webp")
            return ".webp";
        return ".bin";
    }

    std::filesystem::path resolveRelativeTo(const std::filesystem::path& baseDirectory,
                                            const std::string& uri)
    {
        if (uri.empty())
            return {};
        std::filesystem::path path(uri);
        if (path.is_absolute())
            return path.lexically_normal();
        return (baseDirectory / path).lexically_normal();
    }

    struct BufferView
    {
        int32_t buffer = -1;
        size_t byteOffset = 0;
        size_t byteLength = 0;
        size_t byteStride = 0;
    };

    struct Accessor
    {
        int32_t bufferView = -1;
        size_t byteOffset = 0;
        size_t count = 0;
        int32_t componentType = 0;
        std::string type;
        bool normalized = false;
    };

    struct GltfData
    {
        JsonValue root;
        std::filesystem::path sourcePath;
        std::filesystem::path baseDirectory;
        std::vector<std::vector<uint8_t>> buffers;
        std::vector<BufferView> bufferViews;
        std::vector<Accessor> accessors;
    };

    size_t componentSize(int32_t componentType)
    {
        switch (componentType)
        {
            case 5120:
            case 5121:
                return 1;
            case 5122:
            case 5123:
                return 2;
            case 5125:
            case 5126:
                return 4;
        }
        return 0;
    }

    size_t componentCountForType(const std::string& type)
    {
        if (type == "SCALAR")
            return 1;
        if (type == "VEC2")
            return 2;
        if (type == "VEC3")
            return 3;
        if (type == "VEC4")
            return 4;
        if (type == "MAT4")
            return 16;
        return 0;
    }

    float normalizedIntegerValue(int64_t value, int32_t componentType)
    {
        switch (componentType)
        {
            case 5120:
                return std::max(static_cast<float>(value) / 127.0f, -1.0f);
            case 5121:
                return static_cast<float>(value) / 255.0f;
            case 5122:
                return std::max(static_cast<float>(value) / 32767.0f, -1.0f);
            case 5123:
                return static_cast<float>(value) / 65535.0f;
        }
        return static_cast<float>(value);
    }

    bool readAccessorComponent(const GltfData& data,
                               const Accessor& accessor,
                               size_t elementIndex,
                               size_t componentIndex,
                               float& value)
    {
        if (accessor.bufferView < 0 || static_cast<size_t>(accessor.bufferView) >= data.bufferViews.size())
            return false;
        const BufferView& view = data.bufferViews[static_cast<size_t>(accessor.bufferView)];
        if (view.buffer < 0 || static_cast<size_t>(view.buffer) >= data.buffers.size())
            return false;

        const size_t componentBytes = componentSize(accessor.componentType);
        const size_t componentCount = componentCountForType(accessor.type);
        if (componentBytes == 0 || componentCount == 0 || componentIndex >= componentCount)
            return false;

        const size_t stride = view.byteStride != 0 ? view.byteStride : componentBytes * componentCount;
        const size_t offset =
            view.byteOffset + accessor.byteOffset + elementIndex * stride + componentIndex * componentBytes;
        const std::vector<uint8_t>& buffer = data.buffers[static_cast<size_t>(view.buffer)];
        if (offset + componentBytes > buffer.size())
            return false;

        const uint8_t* ptr = buffer.data() + offset;
        switch (accessor.componentType)
        {
            case 5120:
            {
                int8_t raw = 0;
                std::memcpy(&raw, ptr, sizeof(raw));
                value = accessor.normalized ? normalizedIntegerValue(raw, accessor.componentType)
                                            : static_cast<float>(raw);
                return true;
            }
            case 5121:
            {
                uint8_t raw = 0;
                std::memcpy(&raw, ptr, sizeof(raw));
                value = accessor.normalized ? normalizedIntegerValue(raw, accessor.componentType)
                                            : static_cast<float>(raw);
                return true;
            }
            case 5122:
            {
                int16_t raw = 0;
                std::memcpy(&raw, ptr, sizeof(raw));
                value = accessor.normalized ? normalizedIntegerValue(raw, accessor.componentType)
                                            : static_cast<float>(raw);
                return true;
            }
            case 5123:
            {
                uint16_t raw = 0;
                std::memcpy(&raw, ptr, sizeof(raw));
                value = accessor.normalized ? normalizedIntegerValue(raw, accessor.componentType)
                                            : static_cast<float>(raw);
                return true;
            }
            case 5125:
            {
                uint32_t raw = 0;
                std::memcpy(&raw, ptr, sizeof(raw));
                value = static_cast<float>(raw);
                return true;
            }
            case 5126:
                std::memcpy(&value, ptr, sizeof(value));
                return true;
        }
        return false;
    }

    bool readAccessorElement(const GltfData& data,
                             int32_t accessorIndex,
                             size_t elementIndex,
                             std::vector<float>& values)
    {
        if (accessorIndex < 0 || static_cast<size_t>(accessorIndex) >= data.accessors.size())
            return false;
        const Accessor& accessor = data.accessors[static_cast<size_t>(accessorIndex)];
        const size_t componentCount = componentCountForType(accessor.type);
        if (elementIndex >= accessor.count || componentCount == 0)
            return false;

        values.assign(componentCount, 0.0f);
        for (size_t component = 0; component < componentCount; ++component)
        {
            if (!readAccessorComponent(data, accessor, elementIndex, component, values[component]))
                return false;
        }
        return true;
    }

    bool readIndex(const GltfData& data,
                   int32_t accessorIndex,
                   size_t elementIndex,
                   uint32_t& value)
    {
        std::vector<float> components;
        if (!readAccessorElement(data, accessorIndex, elementIndex, components) || components.empty())
            return false;
        value = static_cast<uint32_t>(components[0]);
        return true;
    }

    int32_t attributeAccessor(const JsonValue& attributes, const std::string& name)
    {
        return intValue(member(attributes, name), -1);
    }

    bool appendAccessorArray(const JsonValue& array,
                             std::vector<Accessor>& accessors)
    {
        if (!array.isArray())
            return true;
        accessors.reserve(array.array.size());
        for (const JsonValue& value : array.array)
        {
            Accessor accessor;
            accessor.bufferView = intValue(member(value, "bufferView"), -1);
            accessor.byteOffset = uintValue(member(value, "byteOffset"), 0);
            accessor.count = uintValue(member(value, "count"), 0);
            accessor.componentType = intValue(member(value, "componentType"), 0);
            accessor.type = stringValue(member(value, "type"));
            accessor.normalized = boolValue(member(value, "normalized"), false);
            accessors.push_back(std::move(accessor));
        }
        return true;
    }

    bool appendBufferViews(const JsonValue& array,
                           std::vector<BufferView>& bufferViews)
    {
        if (!array.isArray())
            return true;
        bufferViews.reserve(array.array.size());
        for (const JsonValue& value : array.array)
        {
            BufferView view;
            view.buffer = intValue(member(value, "buffer"), -1);
            view.byteOffset = uintValue(member(value, "byteOffset"), 0);
            view.byteLength = uintValue(member(value, "byteLength"), 0);
            view.byteStride = uintValue(member(value, "byteStride"), 0);
            bufferViews.push_back(view);
        }
        return true;
    }

    bool readGlb(const std::filesystem::path& sourcePath,
                 std::string& jsonText,
                 std::vector<uint8_t>& binChunk,
                 std::string& error)
    {
        std::vector<uint8_t> bytes;
        if (!readFileBytes(sourcePath, bytes))
        {
            error = "Could not read GLB file";
            return false;
        }
        if (bytes.size() < 12u ||
            readU32LE(bytes, 0u) != 0x46546C67u ||
            readU32LE(bytes, 4u) != 2u)
        {
            error = "GLB header is invalid or unsupported";
            return false;
        }

        size_t cursor = 12u;
        while (cursor + 8u <= bytes.size())
        {
            const uint32_t chunkLength = readU32LE(bytes, cursor);
            const uint32_t chunkType = readU32LE(bytes, cursor + 4u);
            cursor += 8u;
            if (cursor + chunkLength > bytes.size())
            {
                error = "GLB chunk range is invalid";
                return false;
            }
            if (chunkType == 0x4E4F534Au)
            {
                jsonText.assign(
                    reinterpret_cast<const char*>(bytes.data() + cursor),
                    chunkLength);
            }
            else if (chunkType == 0x004E4942u)
            {
                binChunk.assign(bytes.begin() + static_cast<std::ptrdiff_t>(cursor),
                                bytes.begin() + static_cast<std::ptrdiff_t>(cursor + chunkLength));
            }
            cursor += chunkLength;
        }

        if (jsonText.empty())
        {
            error = "GLB file does not contain a JSON chunk";
            return false;
        }
        return true;
    }

    bool loadBuffers(GltfData& data,
                     const JsonValue& buffersValue,
                     const std::vector<uint8_t>& binChunk,
                     AssetImportResult& result)
    {
        if (!buffersValue.isArray())
            return true;
        data.buffers.resize(buffersValue.array.size());
        for (size_t i = 0; i < buffersValue.array.size(); ++i)
        {
            const JsonValue& buffer = buffersValue.array[i];
            const std::string uri = stringValue(member(buffer, "uri"));
            if (uri.empty() && i == 0 && !binChunk.empty())
            {
                data.buffers[i] = binChunk;
                continue;
            }
            if (uri.starts_with("data:"))
            {
                std::string mimeType;
                if (!decodeDataUri(uri, mimeType, data.buffers[i]))
                {
                    addDiagnostic(result, AssetDiagnosticSeverity::Error, "GLTF_BUFFER_DATA_URI_INVALID",
                                  "glTF buffer data URI could not be decoded", data.sourcePath);
                    return false;
                }
                continue;
            }

            const std::filesystem::path bufferPath = resolveRelativeTo(data.baseDirectory, uri);
            if (!readFileBytes(bufferPath, data.buffers[i]))
            {
                addDiagnostic(result, AssetDiagnosticSeverity::Error, "GLTF_BUFFER_MISSING",
                              "glTF external buffer could not be read: " + bufferPath.string(),
                              data.sourcePath);
                return false;
            }
            addDependency(result, bufferPath, AssetDependencyType::SourceFile);
        }
        return true;
    }

    std::vector<uint8_t> bytesFromBufferView(const GltfData& data, int32_t bufferViewIndex)
    {
        if (bufferViewIndex < 0 || static_cast<size_t>(bufferViewIndex) >= data.bufferViews.size())
            return {};
        const BufferView& view = data.bufferViews[static_cast<size_t>(bufferViewIndex)];
        if (view.buffer < 0 || static_cast<size_t>(view.buffer) >= data.buffers.size())
            return {};
        const std::vector<uint8_t>& buffer = data.buffers[static_cast<size_t>(view.buffer)];
        if (view.byteOffset > buffer.size() || view.byteLength > buffer.size() - view.byteOffset)
            return {};
        return {
            buffer.begin() + static_cast<std::ptrdiff_t>(view.byteOffset),
            buffer.begin() + static_cast<std::ptrdiff_t>(view.byteOffset + view.byteLength)
        };
    }

    void addTextureDependency(AssetImportResult& result,
                              const ImportedTexture& texture,
                              const std::filesystem::path& sourcePath)
    {
        if (texture.source == ImportedTextureSource::ExternalFile && !texture.sourcePath.empty())
        {
            addDependency(result, texture.sourcePath, AssetDependencyType::Texture);
            if (!std::filesystem::exists(texture.sourcePath))
            {
                addDiagnostic(result, AssetDiagnosticSeverity::Warning, "ASSET_DEPENDENCY_MISSING",
                              "Referenced glTF texture dependency is missing: " + texture.sourcePath.string(),
                              sourcePath);
            }
        }
    }

    void importTextures(const GltfData& data, AssetImportResult& result)
    {
        const JsonValue* textures = member(data.root, "textures");
        const JsonValue* images = member(data.root, "images");
        if (textures == nullptr || !textures->isArray())
            return;

        result.textures.resize(textures->array.size());
        for (size_t textureIndex = 0; textureIndex < textures->array.size(); ++textureIndex)
        {
            const JsonValue& textureValue = textures->array[textureIndex];
            const int32_t imageIndex = intValue(member(textureValue, "source"), -1);
            ImportedTexture imported;
            imported.debugName = "texture_" + std::to_string(textureIndex);
            imported.logicalPath = imported.debugName;

            const JsonValue* image = images == nullptr ? nullptr : at(*images, static_cast<size_t>(imageIndex));
            if (image != nullptr)
            {
                imported.debugName = stringValue(member(*image, "name"), imported.debugName);
                const std::string uri = stringValue(member(*image, "uri"));
                imported.mimeType = stringValue(member(*image, "mimeType"));
                if (!uri.empty() && uri.starts_with("data:"))
                {
                    imported.source = ImportedTextureSource::EmbeddedBytes;
                    if (!decodeDataUri(uri, imported.mimeType, imported.embeddedBytes))
                    {
                        addDiagnostic(result, AssetDiagnosticSeverity::Warning, "GLTF_EMBEDDED_IMAGE_INVALID",
                                      "Embedded glTF image data URI could not be decoded", data.sourcePath);
                    }
                    imported.logicalPath =
                        data.sourcePath.stem().string() + "_image_" + std::to_string(imageIndex)
                        + extensionForMimeType(imported.mimeType);
                }
                else if (!uri.empty())
                {
                    imported.source = ImportedTextureSource::ExternalFile;
                    imported.sourcePath = resolveRelativeTo(data.baseDirectory, uri);
                    imported.logicalPath = std::filesystem::path(uri).generic_string();
                    if (imported.mimeType.empty())
                        imported.mimeType = imported.sourcePath.extension().string();
                }
                else
                {
                    const int32_t bufferView = intValue(member(*image, "bufferView"), -1);
                    imported.source = ImportedTextureSource::EmbeddedBytes;
                    imported.embeddedBytes = bytesFromBufferView(data, bufferView);
                    imported.logicalPath =
                        data.sourcePath.stem().string() + "_image_" + std::to_string(imageIndex)
                        + extensionForMimeType(imported.mimeType);
                    if (imported.embeddedBytes.empty())
                    {
                        addDiagnostic(result, AssetDiagnosticSeverity::Warning, "GLTF_EMBEDDED_IMAGE_INVALID",
                                      "Embedded glTF image bufferView could not be read", data.sourcePath);
                    }
                }
            }

            addTextureDependency(result, imported, data.sourcePath);
            result.textures[textureIndex] = std::move(imported);
        }
    }

    void setTextureRole(AssetImportResult& result,
                        int32_t textureIndex,
                        MaterialTextureRole role,
                        TextureColorSpace colorSpace)
    {
        if (textureIndex < 0 || static_cast<size_t>(textureIndex) >= result.textures.size())
            return;
        ImportedTexture& texture = result.textures[static_cast<size_t>(textureIndex)];
        texture.intendedRole = role;
        texture.colorSpace = colorSpace;
    }

    void importMaterials(const GltfData& data, AssetImportResult& result)
    {
        const JsonValue* materials = member(data.root, "materials");
        if (materials == nullptr || !materials->isArray())
            return;

        result.materials.reserve(materials->array.size());
        for (size_t materialIndex = 0; materialIndex < materials->array.size(); ++materialIndex)
        {
            const JsonValue& materialValue = materials->array[materialIndex];
            ImportedMaterial material;
            material.name = stringValue(member(materialValue, "name"), "material_" + std::to_string(materialIndex));

            const JsonValue* pbr = member(materialValue, "pbrMetallicRoughness");
            if (pbr != nullptr)
            {
                material.baseColor = vec4Value(member(*pbr, "baseColorFactor"), {1.0f, 1.0f, 1.0f, 1.0f});
                material.metallic = floatValue(member(*pbr, "metallicFactor"), 1.0f);
                material.roughness = floatValue(member(*pbr, "roughnessFactor"), 1.0f);
                if (const JsonValue* texture = member(*pbr, "baseColorTexture"))
                {
                    material.baseColorTextureIndex = intValue(member(*texture, "index"), -1);
                    setTextureRole(result, material.baseColorTextureIndex,
                                   MaterialTextureRole::BaseColor, TextureColorSpace::SRgb);
                }
                if (const JsonValue* texture = member(*pbr, "metallicRoughnessTexture"))
                {
                    material.metallicRoughnessTextureIndex = intValue(member(*texture, "index"), -1);
                    setTextureRole(result, material.metallicRoughnessTextureIndex,
                                   MaterialTextureRole::MetallicRoughness, TextureColorSpace::Linear);
                }
            }

            if (const JsonValue* texture = member(materialValue, "normalTexture"))
            {
                material.normalTextureIndex = intValue(member(*texture, "index"), -1);
                material.normalScale = floatValue(member(*texture, "scale"), 1.0f);
                setTextureRole(result, material.normalTextureIndex,
                               MaterialTextureRole::Normal, TextureColorSpace::Linear);
            }
            if (const JsonValue* texture = member(materialValue, "occlusionTexture"))
            {
                material.ambientOcclusionTextureIndex = intValue(member(*texture, "index"), -1);
                material.ambientOcclusionStrength = floatValue(member(*texture, "strength"), 1.0f);
                setTextureRole(result, material.ambientOcclusionTextureIndex,
                               MaterialTextureRole::AmbientOcclusion, TextureColorSpace::Linear);
            }
            if (const JsonValue* texture = member(materialValue, "emissiveTexture"))
            {
                material.emissiveTextureIndex = intValue(member(*texture, "index"), -1);
                setTextureRole(result, material.emissiveTextureIndex,
                               MaterialTextureRole::Emissive, TextureColorSpace::SRgb);
            }

            material.emissiveFactor =
                vec3Value(member(materialValue, "emissiveFactor"), {0.0f, 0.0f, 0.0f});
            if (const JsonValue* extensions = member(materialValue, "extensions"))
            {
                if (const JsonValue* emissiveStrength = member(*extensions, "KHR_materials_emissive_strength"))
                    material.emissiveStrength = floatValue(member(*emissiveStrength, "emissiveStrength"), 1.0f);
            }

            const std::string alphaMode = stringValue(member(materialValue, "alphaMode"), "OPAQUE");
            material.renderState.alphaCutoff = floatValue(member(materialValue, "alphaCutoff"), 0.5f);
            if (alphaMode == "BLEND")
            {
                material.renderState.alphaMode = MaterialAlphaMode::Blend;
                material.renderState.depthWrite = false;
            }
            else if (alphaMode == "MASK")
            {
                material.renderState.alphaMode = MaterialAlphaMode::Mask;
            }
            material.renderState.doubleSided = boolValue(member(materialValue, "doubleSided"), false);
            result.materials.push_back(std::move(material));
        }
    }

    void warnUnsupportedTopLevel(const GltfData& data, AssetImportResult& result)
    {
        const std::unordered_set<std::string> supportedExtensions = {
            "KHR_materials_emissive_strength"
        };
        const JsonValue* extensionsRequired = member(data.root, "extensionsRequired");
        if (extensionsRequired != nullptr && extensionsRequired->isArray())
        {
            for (const JsonValue& extension : extensionsRequired->array)
            {
                const std::string name = stringValue(&extension);
                if (!name.empty() && !supportedExtensions.contains(name))
                {
                    addDiagnostic(result, AssetDiagnosticSeverity::Error, "GLTF_UNSUPPORTED_EXTENSION",
                                  "Required glTF extension is unsupported: " + name, data.sourcePath);
                }
            }
        }

        const JsonValue* extensionsUsed = member(data.root, "extensionsUsed");
        if (extensionsUsed != nullptr && extensionsUsed->isArray())
        {
            for (const JsonValue& extension : extensionsUsed->array)
            {
                const std::string name = stringValue(&extension);
                if (name == "KHR_lights_punctual")
                {
                    addDiagnostic(result, AssetDiagnosticSeverity::Warning, "GLTF_LIGHT_SKIPPED",
                                  "glTF punctual lights are not imported", data.sourcePath);
                }
                else if (!name.empty() && !supportedExtensions.contains(name))
                {
                    addDiagnostic(result, AssetDiagnosticSeverity::Warning, "GLTF_UNSUPPORTED_EXTENSION",
                                  "Optional glTF extension is not imported: " + name, data.sourcePath);
                }
            }
        }

        if (const JsonValue* skins = member(data.root, "skins"); skins != nullptr && skins->isArray() && !skins->array.empty())
        {
            addDiagnostic(result, AssetDiagnosticSeverity::Warning, "GLTF_SKINNING_NOT_IMPLEMENTED",
                          "glTF skins are not imported", data.sourcePath);
        }
        if (const JsonValue* animations = member(data.root, "animations");
            animations != nullptr && animations->isArray() && !animations->array.empty())
        {
            addDiagnostic(result, AssetDiagnosticSeverity::Warning, "GLTF_ANIMATION_SKIPPED",
                          "glTF animations are not imported", data.sourcePath);
        }
        if (const JsonValue* cameras = member(data.root, "cameras"); cameras != nullptr && cameras->isArray() && !cameras->array.empty())
        {
            addDiagnostic(result, AssetDiagnosticSeverity::Warning, "GLTF_CAMERA_SKIPPED",
                          "glTF cameras are not imported", data.sourcePath);
        }
    }

    bool importMeshes(const GltfData& data, bool flipTexCoordV, AssetImportResult& result)
    {
        const JsonValue* meshes = member(data.root, "meshes");
        if (meshes == nullptr || !meshes->isArray())
            return true;

        result.meshes.reserve(meshes->array.size());
        for (size_t meshIndex = 0; meshIndex < meshes->array.size(); ++meshIndex)
        {
            const JsonValue& meshValue = meshes->array[meshIndex];
            ImportedMesh mesh;
            mesh.debugName = stringValue(member(meshValue, "name"), "mesh_" + std::to_string(meshIndex));
            mesh.sourcePath = data.sourcePath;

            bool allNormalsPresent = true;
            bool allTangentsPresent = true;
            bool allTexCoordsPresent = true;
            const JsonValue* primitives = member(meshValue, "primitives");
            if (primitives == nullptr || !primitives->isArray())
                continue;

            for (size_t primitiveIndex = 0; primitiveIndex < primitives->array.size(); ++primitiveIndex)
            {
                const JsonValue& primitive = primitives->array[primitiveIndex];
                const uint32_t mode = uintValue(member(primitive, "mode"), 4u);
                if (mode != 4u)
                {
                    addDiagnostic(result, AssetDiagnosticSeverity::Error, "GLTF_PRIMITIVE_MODE_UNSUPPORTED",
                                  "Only glTF triangle primitives are supported", data.sourcePath);
                    return false;
                }

                const JsonValue* attributes = member(primitive, "attributes");
                if (attributes == nullptr || !attributes->isObject())
                {
                    addDiagnostic(result, AssetDiagnosticSeverity::Error, "GLTF_PRIMITIVE_MISSING_ATTRIBUTES",
                                  "glTF primitive is missing attributes", data.sourcePath);
                    return false;
                }

                const int32_t positionAccessor = attributeAccessor(*attributes, "POSITION");
                if (positionAccessor < 0 || static_cast<size_t>(positionAccessor) >= data.accessors.size())
                {
                    addDiagnostic(result, AssetDiagnosticSeverity::Error, "GLTF_POSITION_MISSING",
                                  "glTF primitive is missing POSITION data", data.sourcePath);
                    return false;
                }

                const int32_t normalAccessor = attributeAccessor(*attributes, "NORMAL");
                const int32_t tangentAccessor = attributeAccessor(*attributes, "TANGENT");
                const int32_t texCoordAccessor = attributeAccessor(*attributes, "TEXCOORD_0");
                const int32_t colorAccessor = attributeAccessor(*attributes, "COLOR_0");
                const int32_t indexAccessor = intValue(member(primitive, "indices"), -1);
                const Accessor& positions = data.accessors[static_cast<size_t>(positionAccessor)];
                const size_t vertexOffset = mesh.vertices.size();
                const uint32_t firstIndex = static_cast<uint32_t>(mesh.indices.size());

                if (normalAccessor < 0)
                    allNormalsPresent = false;
                if (tangentAccessor < 0)
                    allTangentsPresent = false;
                if (texCoordAccessor < 0)
                    allTexCoordsPresent = false;

                if (member(*attributes, "TEXCOORD_1") != nullptr)
                {
                    addDiagnostic(result, AssetDiagnosticSeverity::Warning, "GLTF_MULTIPLE_UV_SETS_IGNORED",
                                  "Additional glTF UV sets are not imported", data.sourcePath);
                }
                if (member(*attributes, "JOINTS_0") != nullptr || member(*attributes, "WEIGHTS_0") != nullptr)
                {
                    addDiagnostic(result, AssetDiagnosticSeverity::Warning, "GLTF_SKINNING_NOT_IMPLEMENTED",
                                  "glTF joint and weight attributes are not imported", data.sourcePath);
                }
                if (const JsonValue* targets = member(primitive, "targets");
                    targets != nullptr && targets->isArray() && !targets->array.empty())
                {
                    addDiagnostic(result, AssetDiagnosticSeverity::Warning, "GLTF_MORPH_TARGETS_NOT_IMPLEMENTED",
                                  "glTF morph targets are not imported", data.sourcePath);
                }

                std::vector<float> components;
                for (size_t vertexIndex = 0; vertexIndex < positions.count; ++vertexIndex)
                {
                    Vertex vertex;
                    if (!readAccessorElement(data, positionAccessor, vertexIndex, components) ||
                        components.size() < 3u)
                    {
                        addDiagnostic(result, AssetDiagnosticSeverity::Error, "GLTF_POSITION_INVALID",
                                      "glTF POSITION accessor could not be read", data.sourcePath);
                        return false;
                    }
                    vertex.pos = {components[0], components[1], components[2]};

                    if (normalAccessor >= 0 &&
                        readAccessorElement(data, normalAccessor, vertexIndex, components) &&
                        components.size() >= 3u)
                    {
                        vertex.normal = safeNormalize(
                            {components[0], components[1], components[2]},
                            {0.0f, 0.0f, 1.0f});
                    }

                    if (tangentAccessor >= 0 &&
                        readAccessorElement(data, tangentAccessor, vertexIndex, components) &&
                        components.size() >= 4u)
                    {
                        vertex.tangent = {
                            components[0],
                            components[1],
                            components[2],
                            flipTexCoordV ? (components[3] * -1.0f) : components[3]
                        };
                    }

                    if (colorAccessor >= 0 &&
                        readAccessorElement(data, colorAccessor, vertexIndex, components))
                    {
                        vertex.color = components.size() >= 4u
                            ? glm::vec4(components[0], components[1], components[2], components[3])
                            : glm::vec4(components[0], components[1], components[2], 1.0f);
                    }

                    if (texCoordAccessor >= 0 &&
                        readAccessorElement(data, texCoordAccessor, vertexIndex, components) &&
                        components.size() >= 2u)
                    {
                        const float v = components[1];
                        vertex.texCoord = {
                            components[0],
                            flipTexCoordV ? 1.0f - v : v
                        };
                    }

                    mesh.vertices.push_back(vertex);
                }

                if (indexAccessor >= 0)
                {
                    if (static_cast<size_t>(indexAccessor) >= data.accessors.size())
                    {
                        addDiagnostic(result, AssetDiagnosticSeverity::Error, "GLTF_INDEX_INVALID",
                                      "glTF index accessor is invalid", data.sourcePath);
                        return false;
                    }
                    const Accessor& indices = data.accessors[static_cast<size_t>(indexAccessor)];
                    for (size_t index = 0; index < indices.count; ++index)
                    {
                        uint32_t value = 0;
                        if (!readIndex(data, indexAccessor, index, value) || value >= positions.count)
                        {
                            addDiagnostic(result, AssetDiagnosticSeverity::Error, "GLTF_INDEX_INVALID",
                                          "glTF index data is invalid", data.sourcePath);
                            return false;
                        }
                        mesh.indices.push_back(static_cast<uint32_t>(vertexOffset) + value);
                    }
                }
                else
                {
                    for (size_t index = 0; index < positions.count; ++index)
                        mesh.indices.push_back(static_cast<uint32_t>(vertexOffset + index));
                }

                ImportedMeshPrimitive importedPrimitive;
                importedPrimitive.name = mesh.debugName + "_primitive_" + std::to_string(primitiveIndex);
                importedPrimitive.firstIndex = firstIndex;
                importedPrimitive.indexCount = static_cast<uint32_t>(mesh.indices.size()) - firstIndex;
                importedPrimitive.materialIndex = intValue(member(primitive, "material"), -1);
                if (importedPrimitive.materialIndex >= 0 &&
                    static_cast<size_t>(importedPrimitive.materialIndex) < result.materials.size())
                {
                    importedPrimitive.materialName =
                        result.materials[static_cast<size_t>(importedPrimitive.materialIndex)].name;
                }
                mesh.primitives.push_back(std::move(importedPrimitive));
            }

            mesh.hadMissingNormals = !allNormalsPresent;
            mesh.hadMissingTexCoords = !allTexCoordsPresent;
            mesh.sourceAttributes = VertexAttributeFlags::Position | VertexAttributeFlags::Color;
            if (allNormalsPresent && !mesh.indices.empty())
                mesh.sourceAttributes |= VertexAttributeFlags::Normal;
            if (allTangentsPresent && !mesh.indices.empty())
                mesh.sourceAttributes |= VertexAttributeFlags::Tangent;
            if (allTexCoordsPresent && !mesh.indices.empty())
                mesh.sourceAttributes |= VertexAttributeFlags::UV0;

            if (mesh.hadMissingNormals)
            {
                addDiagnostic(result, AssetDiagnosticSeverity::Warning, "GLTF_MISSING_NORMALS",
                              "glTF mesh has primitives without NORMAL data", data.sourcePath);
            }
            if (!allTangentsPresent)
            {
                addDiagnostic(result, AssetDiagnosticSeverity::Warning, "GLTF_MISSING_TANGENTS",
                              "glTF mesh has primitives without TANGENT data", data.sourcePath);
            }
            if (mesh.hadMissingTexCoords)
            {
                addDiagnostic(result, AssetDiagnosticSeverity::Warning, "GLTF_MISSING_UV0",
                              "glTF mesh has primitives without TEXCOORD_0 data", data.sourcePath);
            }

            result.meshes.push_back(std::move(mesh));
        }
        return true;
    }

    glm::mat4 nodeTransform(const JsonValue& node)
    {
        if (const JsonValue* matrix = member(node, "matrix");
            matrix != nullptr && matrix->isArray() && matrix->array.size() >= 16u)
        {
            glm::mat4 result(1.0f);
            for (size_t col = 0; col < 4u; ++col)
            {
                for (size_t row = 0; row < 4u; ++row)
                    result[static_cast<int>(col)][static_cast<int>(row)] =
                        floatValue(&matrix->array[col * 4u + row], col == row ? 1.0f : 0.0f);
            }
            return result;
        }

        const glm::vec3 translation = vec3Value(member(node, "translation"), {0.0f, 0.0f, 0.0f});
        const glm::vec4 rotationValue = vec4Value(member(node, "rotation"), {0.0f, 0.0f, 0.0f, 1.0f});
        const glm::vec3 scale = vec3Value(member(node, "scale"), {1.0f, 1.0f, 1.0f});
        const glm::quat rotation(rotationValue.w, rotationValue.x, rotationValue.y, rotationValue.z);
        return glm::translate(glm::mat4(1.0f), translation)
            * glm::mat4_cast(rotation)
            * glm::scale(glm::mat4(1.0f), scale);
    }

    void importNodes(const GltfData& data, AssetImportResult& result)
    {
        const JsonValue* nodes = member(data.root, "nodes");
        if (nodes == nullptr || !nodes->isArray())
            return;

        result.nodes.resize(nodes->array.size());
        for (size_t nodeIndex = 0; nodeIndex < nodes->array.size(); ++nodeIndex)
        {
            const JsonValue& node = nodes->array[nodeIndex];
            ImportedNode imported;
            imported.name = stringValue(member(node, "name"), "node_" + std::to_string(nodeIndex));
            imported.meshIndex = intValue(member(node, "mesh"), -1);
            imported.localTransform = nodeTransform(node);
            result.nodes[nodeIndex] = std::move(imported);

            if (member(node, "skin") != nullptr)
            {
                addDiagnostic(result, AssetDiagnosticSeverity::Warning, "GLTF_SKINNING_NOT_IMPLEMENTED",
                              "glTF node skin binding is not imported", data.sourcePath);
            }
            if (member(node, "camera") != nullptr)
            {
                addDiagnostic(result, AssetDiagnosticSeverity::Warning, "GLTF_CAMERA_SKIPPED",
                              "glTF node camera binding is not imported", data.sourcePath);
            }
        }

        for (size_t nodeIndex = 0; nodeIndex < nodes->array.size(); ++nodeIndex)
        {
            const JsonValue* children = member(nodes->array[nodeIndex], "children");
            if (children == nullptr || !children->isArray())
                continue;
            for (const JsonValue& child : children->array)
            {
                const int32_t childIndex = intValue(&child, -1);
                if (childIndex >= 0 && static_cast<size_t>(childIndex) < result.nodes.size())
                    result.nodes[static_cast<size_t>(childIndex)].parentIndex = static_cast<int32_t>(nodeIndex);
            }
        }
    }
}

std::string_view GltfAssetImporter::name() const
{
    return "gltf";
}

uint32_t GltfAssetImporter::version() const
{
    return 1;
}

AssetImportCapability GltfAssetImporter::capabilities() const
{
    return AssetImportCapability::Meshes
        | AssetImportCapability::Materials
        | AssetImportCapability::Textures
        | AssetImportCapability::Nodes;
}

bool GltfAssetImporter::supports(const std::filesystem::path& sourcePath) const
{
    std::string extension = sourcePath.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char ch)
    {
        return static_cast<char>(std::tolower(ch));
    });
    return extension == ".gltf" || extension == ".glb";
}

AssetImportResult GltfAssetImporter::importAsset(const AssetImportRequest& request) const
{
    AssetImportResult result;
    const std::filesystem::path sourcePath = request.sourcePath;
    addDependency(result, sourcePath, AssetDependencyType::SourceFile);

    if (sourcePath.empty() || !std::filesystem::exists(sourcePath))
    {
        addDiagnostic(result, AssetDiagnosticSeverity::Error, "ASSET_SOURCE_MISSING",
                      "glTF source file does not exist: " + sourcePath.string(), sourcePath);
        return result;
    }

    std::string jsonText;
    std::vector<uint8_t> binChunk;
    std::string error;
    std::string extension = sourcePath.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char ch)
    {
        return static_cast<char>(std::tolower(ch));
    });

    if (extension == ".glb")
    {
        if (!readGlb(sourcePath, jsonText, binChunk, error))
        {
            addDiagnostic(result, AssetDiagnosticSeverity::Error, "GLTF_READ_FAILED", error, sourcePath);
            return result;
        }
    }
    else
    {
        jsonText = readFileText(sourcePath);
        if (jsonText.empty())
        {
            addDiagnostic(result, AssetDiagnosticSeverity::Error, "GLTF_READ_FAILED",
                          "Could not read glTF JSON file", sourcePath);
            return result;
        }
    }

    GltfData data;
    data.sourcePath = sourcePath;
    data.baseDirectory = sourcePath.parent_path();
    JsonParser parser(jsonText);
    if (!parser.parse(data.root, error))
    {
        addDiagnostic(result, AssetDiagnosticSeverity::Error, "GLTF_JSON_INVALID", error, sourcePath);
        return result;
    }

    warnUnsupportedTopLevel(data, result);
    if (result.hasErrors())
        return result;

    if (const JsonValue* bufferViews = member(data.root, "bufferViews"))
        appendBufferViews(*bufferViews, data.bufferViews);
    if (const JsonValue* accessors = member(data.root, "accessors"))
        appendAccessorArray(*accessors, data.accessors);
    const JsonValue* buffers = member(data.root, "buffers");
    if (buffers != nullptr && !loadBuffers(data, *buffers, binChunk, result))
        return result;

    if (request.options.includeSourceMaterials &&
        hasImportCapability(request.requestedCapabilities, AssetImportCapability::Textures))
    {
        importTextures(data, result);
    }
    if (request.options.includeSourceMaterials &&
        hasImportCapability(request.requestedCapabilities, AssetImportCapability::Materials))
    {
        importMaterials(data, result);
    }
    if (hasImportCapability(request.requestedCapabilities, AssetImportCapability::Meshes))
    {
        if (!importMeshes(data, request.options.flipTexCoordV, result))
            return result;
    }
    if (hasImportCapability(request.requestedCapabilities, AssetImportCapability::Nodes))
        importNodes(data, result);

    return result;
}
}
