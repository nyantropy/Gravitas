#include "GltfSourceReader.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cctype>
#include <limits>
#include <utility>

#include "GltfSourceUtilities.h"
#include "GtsJsonParser.h"

namespace gts::gltf
{
    [[noreturn]] void fail(std::string code, std::string message, std::string location)
    {
        throw DecodeError{
            {GtsModelDiagnosticSeverity::Error, std::move(code), std::move(message), std::move(location)}};
    }
    void object(const GtsJsonValue& value, const std::string& location)
    {
        if (!value.isObject())
            fail("GLTF_SCHEMA", "Expected an object", location);
    }
    const GtsJsonValue::Array& array(const GtsJsonValue* value, const std::string& location)
    {
        static const GtsJsonValue::Array empty;
        if (!value)
            return empty;
        if (!value->isArray())
            fail("GLTF_SCHEMA", "Expected an array", location);
        return value->asArray();
    }
    uint32_t integer(const GtsJsonValue& value, const std::string& location)
    {
        auto number = value.tryUInt32();
        if (!number)
            fail("GLTF_SCHEMA", "Expected a uint32 integer", location);
        return *number;
    }
    uint32_t
    uintField(const GtsJsonValue& value, const char* key, const std::string& location, std::optional<uint32_t> fallback)
    {
        if (const auto* field = value.find(key))
            return integer(*field, location + "." + key);
        if (fallback)
            return *fallback;
        fail("GLTF_SCHEMA", "Missing required integer", location + "." + key);
    }
    std::string
    stringField(const GtsJsonValue& value, const char* key, const std::string& location, const std::string& fallback)
    {
        const auto* field = value.find(key);
        if (!field)
            return fallback;
        if (!field->isString())
            fail("GLTF_SCHEMA", "Expected a string", location + "." + key);
        return field->asString();
    }
    bool boolField(const GtsJsonValue& value, const char* key, const std::string& location, bool fallback)
    {
        const auto* field = value.find(key);
        if (!field)
            return fallback;
        if (!field->isBool())
            fail("GLTF_SCHEMA", "Expected a boolean", location + "." + key);
        return field->asBool();
    }
    float floatField(const GtsJsonValue& value, const char* key, const std::string& location, float fallback)
    {
        const auto* field = value.find(key);
        if (!field)
            return fallback;
        const auto result = field->tryFloat();
        if (!result || !std::isfinite(*result))
            fail("GLTF_SCHEMA", "Expected a finite float", location + "." + key);
        return *result;
    }
    std::vector<float> vectorValue(const GtsJsonValue& value, size_t count, const std::string& location)
    {
        const auto& values = array(&value, location);
        if (values.size() != count)
            fail("GLTF_VECTOR_SIZE", "Incorrect vector component count", location);
        std::vector<float> result;
        for (const auto& item : values)
        {
            const auto number = item.tryFloat();
            if (!number || !std::isfinite(*number))
                fail("GLTF_SCHEMA", "Expected finite vector components", location);
            result.push_back(*number);
        }
        return result;
    }
    std::filesystem::path externalPath(const std::filesystem::path& directory, const std::string& uri)
    {
        std::string decoded;
        auto        hex = [](char ch) -> int
        {
            if (ch >= '0' && ch <= '9')
                return ch - '0';
            if (ch >= 'a' && ch <= 'f')
                return ch - 'a' + 10;
            if (ch >= 'A' && ch <= 'F')
                return ch - 'A' + 10;
            return -1;
        };
        for (size_t i = 0; i < uri.size(); ++i)
        {
            char ch = uri[i];
            if (ch == '%')
            {
                if (i + 2 >= uri.size() || hex(uri[i + 1]) < 0 || hex(uri[i + 2]) < 0)
                    fail("GLTF_URI", "Malformed URI escape", uri);
                ch = static_cast<char>((hex(uri[i + 1]) << 4) | hex(uri[i + 2]));
                i += 2;
            }
            if (ch == '\0')
                fail("GLTF_URI", "NUL in image/buffer URI", uri);
            decoded += ch;
        }
        if (decoded.empty() || uri.find(':') != std::string::npos || uri.starts_with("//") ||
            uri.find_first_of("?#\\") != std::string::npos)
            fail("GLTF_URI_UNSUPPORTED", "Only local relative file URIs are supported", uri);
        return std::filesystem::absolute(directory / std::filesystem::path(decoded)).lexically_normal();
    }
    std::vector<uint8_t> dataUri(const std::string& uri, std::string& mime, const std::string& location)
    {
        const auto comma = uri.find(',');
        if (!uri.starts_with("data:") || comma == std::string::npos || !uri.substr(0, comma).ends_with(";base64"))
            fail("GLTF_DATA_URI", "Expected a base64 data URI", location);
        const auto payload = std::string_view(uri).substr(comma + 1);
        const auto padding = payload.find('=');
        const auto count   = padding == std::string_view::npos ? payload.size() : padding;
        if (payload.empty() || payload.size() % 4 != 0 || payload.size() - count > 2)
            fail("GLTF_DATA_URI", "Invalid base64 length/padding", location);
        for (size_t i = 0; i < payload.size(); ++i)
            if (i < count ? base64Value(payload[i]) < 0 : payload[i] != '=')
                fail("GLTF_DATA_URI", "Invalid base64 encoding", location);
        if (count && ((payload.size() - count == 2 && (base64Value(payload[count - 1]) & 15)) ||
                      (payload.size() - count == 1 && (base64Value(payload[count - 1]) & 3))))
            fail("GLTF_DATA_URI", "Nonzero unused base64 bits", location);
        std::vector<uint8_t> result;
        if (!decodeDataUri(uri, mime, result))
            fail("GLTF_DATA_URI", "Could not decode data URI", location);
        return result;
    }

    const Accessor& SourceDocument::accessor(uint32_t index) const
    {
        if (index >= accessors.size())
            fail("GLTF_ACCESSOR_REFERENCE", "Invalid accessor index", "accessors[" + std::to_string(index) + "]");
        return accessors[index];
    }
    uint32_t SourceDocument::unsignedComponent(const Accessor& a, size_t element, size_t component) const
    {
        const auto&  view   = views[a.view];
        const auto&  buffer = buffers[view.buffer];
        const size_t width  = componentSize(a.componentType);
        const size_t offset = view.offset + a.offset + element * a.stride + component * width;
        uint32_t     value  = 0;
        for (size_t byte = 0; byte < width; ++byte)
            value |= static_cast<uint32_t>(buffer[offset + byte]) << (byte * 8);
        return value;
    }
    float SourceDocument::floatComponent(const Accessor& a, size_t element, size_t component) const
    {
        const uint32_t raw = unsignedComponent(a, element, component);
        if (a.componentType == 5126)
            return std::bit_cast<float>(raw);
        const int64_t value = a.componentType == 5120   ? (raw >= 128 ? static_cast<int64_t>(raw) - 256 : raw)
                              : a.componentType == 5122 ? (raw >= 32768 ? static_cast<int64_t>(raw) - 65536 : raw)
                                                        : raw;
        return a.normalized ? normalizedIntegerValue(value, a.componentType) : static_cast<float>(value);
    }
    std::vector<uint8_t> SourceDocument::viewBytes(uint32_t index) const
    {
        if (index >= views.size())
            fail("GLTF_VIEW_REFERENCE", "Invalid bufferView", "bufferViews");
        const auto& view   = views[index];
        const auto& buffer = buffers[view.buffer];
        return {buffer.begin() + view.offset, buffer.begin() + view.offset + view.length};
    }

    SourceDocument readSource(const std::filesystem::path& path, std::vector<GtsModelDiagnostic>& diagnostics)
    {
        SourceDocument data;
        data.directory = std::filesystem::absolute(path).parent_path();
        std::vector<uint8_t> bytes, bin;
        std::string          extension = path.extension().string();
        std::transform(extension.begin(),
                       extension.end(),
                       extension.begin(),
                       [](unsigned char ch)
                       {
                           return std::tolower(ch);
                       });
        std::string json;
        bool        hasBin = false;
        if (extension == ".glb")
        {
            std::string              error;
            std::vector<std::string> warnings;
            if (!readGlb(path, json, bin, error, &hasBin, &warnings))
                fail("GLTF_GLB_INVALID", error, path.string());
            for (const auto& warning : warnings)
                diagnostics.push_back(
                    {GtsModelDiagnosticSeverity::Warning, "GLTF_GLB_UNKNOWN_CHUNK", warning, path.string()});
        }
        else if (extension == ".gltf")
        {
            if (!readFileBytes(path, bytes))
                fail("GLTF_READ_FAILED", "Cannot read source", path.string());
            json.assign(bytes.begin(), bytes.end());
        }
        else
            fail("GLTF_SOURCE_EXTENSION", "Expected .gltf or .glb", path.string());
        std::string error;
        if (!GtsJsonParser::parse(json, data.root, &error))
            fail("GLTF_JSON_INVALID", error, path.string());
        object(data.root, "root");
        const auto* asset = data.root.find("asset");
        if (!asset)
            fail("GLTF_VERSION", "Missing asset metadata", "asset");
        object(*asset, "asset");
        if (stringField(*asset, "version", "asset") != "2.0" ||
            stringField(*asset, "minVersion", "asset", "2.0") != "2.0")
            fail("GLTF_VERSION", "Only glTF 2.0 is supported", "asset");

        const auto& buffers = array(data.root.find("buffers"), "buffers");
        bool        usedBin = false;
        for (size_t i = 0; i < buffers.size(); ++i)
        {
            const auto  loc    = "buffers[" + std::to_string(i) + "]";
            const auto& source = buffers[i];
            object(source, loc);
            const auto length = uintField(source, "byteLength", loc);
            if (!length)
                fail("GLTF_BUFFER_RANGE", "Buffer byteLength must be positive", loc);
            const auto           uri = stringField(source, "uri", loc);
            std::vector<uint8_t> buffer;
            if (!source.find("uri") && i == 0 && hasBin)
            {
                usedBin = true;
                buffer  = std::move(bin);
                if (buffer.size() < length || buffer.size() - length > 3)
                    fail("GLTF_BUFFER_RANGE",
                         "BIN size must match declared buffer plus at most three padding bytes",
                         loc);
                for (size_t pad = length; pad < buffer.size(); ++pad)
                    if (buffer[pad] != 0)
                        fail("GLTF_GLB_PADDING", "BIN padding must be zero", loc);
            }
            else if (uri.starts_with("data:"))
            {
                std::string mime;
                buffer = dataUri(uri, mime, loc);
            }
            else if (uri.empty() || !readFileBytes(externalPath(data.directory, uri), buffer))
                fail("GLTF_BUFFER_MISSING", "Cannot read external buffer", loc);
            if (length > buffer.size())
                fail("GLTF_BUFFER_RANGE", "Buffer is shorter than byteLength", loc);
            buffer.resize(length);
            data.buffers.push_back(std::move(buffer));
        }
        if (hasBin && !usedBin)
            fail("GLTF_GLB_BIN_UNUSED", "BIN chunk has no corresponding buffer[0]", "buffers");
        for (const auto& source : array(data.root.find("bufferViews"), "bufferViews"))
        {
            const auto loc = "bufferViews[" + std::to_string(data.views.size()) + "]";
            object(source, loc);
            BufferView view;
            view.buffer = uintField(source, "buffer", loc);
            view.offset = uintField(source, "byteOffset", loc, 0);
            view.length = uintField(source, "byteLength", loc);
            view.stride = uintField(source, "byteStride", loc, 0);
            if (view.buffer >= data.buffers.size() || !view.length || view.offset > data.buffers[view.buffer].size() ||
                view.length > data.buffers[view.buffer].size() - view.offset)
                fail("GLTF_VIEW_RANGE", "bufferView exceeds declared buffer range", loc);
            if (source.find("byteStride") && (view.stride < 4 || view.stride > 252 || view.stride % 4))
                fail("GLTF_VIEW_STRIDE", "byteStride must be a multiple of four in [4,252]", loc);
            data.views.push_back(view);
        }
        for (const auto& source : array(data.root.find("accessors"), "accessors"))
        {
            const auto loc = "accessors[" + std::to_string(data.accessors.size()) + "]";
            object(source, loc);
            if (source.find("sparse"))
                fail("GLTF_SPARSE_UNSUPPORTED",
                     "Expand sparse accessors before import; overrides are not yet supported",
                     loc);
            if (!source.find("bufferView"))
                fail("GLTF_ACCESSOR_SOURCE_UNSUPPORTED",
                     "Materialize implicit zero accessors into a bufferView before import",
                     loc);
            Accessor a;
            a.view           = uintField(source, "bufferView", loc);
            a.offset         = uintField(source, "byteOffset", loc, 0);
            a.count          = uintField(source, "count", loc);
            a.componentType  = uintField(source, "componentType", loc);
            a.type           = stringField(source, "type", loc);
            a.components     = componentCountForType(a.type);
            a.normalized     = boolField(source, "normalized", loc);
            const auto width = componentSize(a.componentType);
            if (a.view >= data.views.size() || !a.count || !width || !a.components)
                fail("GLTF_ACCESSOR_LAYOUT", "Unsupported or invalid accessor layout", loc);
            const auto&  view         = data.views[a.view];
            const size_t elementBytes = width * a.components;
            a.stride                  = view.stride ? view.stride : elementBytes;
            if (a.normalized && (a.componentType == 5125 || a.componentType == 5126))
                fail("GLTF_ACCESSOR_LAYOUT", "FLOAT/UNSIGNED_INT cannot be normalized", loc);
            if (a.stride < elementBytes || a.offset % width || view.offset % width || a.stride % width ||
                a.offset > view.length || elementBytes > view.length - a.offset ||
                a.count - 1 > (view.length - a.offset - elementBytes) / a.stride)
                fail("GLTF_ACCESSOR_RANGE", "Accessor alignment, stride, or range exceeds bufferView", loc);
            data.accessors.push_back(a);
        }
        return data;
    }
} // namespace gts::gltf
