#include "GltfSourceUtilities.h"

#include <algorithm>
#include <cctype>
#include <fstream>

namespace gts::gltf
{
    bool readFileBytes(const std::filesystem::path& path, std::vector<uint8_t>& bytes)
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

    uint32_t readU32LE(const std::vector<uint8_t>& bytes, size_t offset)
    {
        if (offset + 4u > bytes.size())
            return 0;
        return static_cast<uint32_t>(bytes[offset + 0u]) | (static_cast<uint32_t>(bytes[offset + 1u]) << 8u) |
               (static_cast<uint32_t>(bytes[offset + 2u]) << 16u) | (static_cast<uint32_t>(bytes[offset + 3u]) << 24u);
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
        int      bits        = 0;
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

    bool decodeDataUri(const std::string& uri, std::string& mimeType, std::vector<uint8_t>& bytes)
    {
        if (!uri.starts_with("data:"))
            return false;
        const size_t comma = uri.find(',');
        if (comma == std::string::npos)
            return false;
        const std::string metadata  = uri.substr(5u, comma - 5u);
        const bool        base64    = metadata.find(";base64") != std::string::npos;
        const size_t      semicolon = metadata.find(';');
        mimeType                    = metadata.substr(0u, semicolon == std::string::npos ? metadata.size() : semicolon);
        if (!base64)
            return false;
        return decodeBase64(std::string_view(uri).substr(comma + 1u), bytes);
    }

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
    bool readGlb(const std::filesystem::path& path,
                 std::string&                 json,
                 std::vector<uint8_t>&        bin,
                 std::string&                 error,
                 bool*                        containsBin,
                 std::vector<std::string>*    warnings)
    {
        std::vector<uint8_t> bytes;
        json.clear();
        bin.clear();
        if (containsBin)
            *containsBin = false;
        auto invalid = [&](const char* message)
        {
            error = message;
            return false;
        };
        if (!readFileBytes(path, bytes))
            return invalid("Cannot read GLB source");
        if (bytes.size() < 20 || readU32LE(bytes, 0) != 0x46546c67 || readU32LE(bytes, 4) != 2 ||
            readU32LE(bytes, 8) != bytes.size())
            return invalid("GLB magic, version, or declared total length is invalid");
        size_t cursor = 12, chunk = 0;
        bool   hasBin = false;
        while (cursor < bytes.size())
        {
            if (bytes.size() - cursor < 8)
                return invalid("Truncated GLB chunk header");
            const auto length = readU32LE(bytes, cursor), type = readU32LE(bytes, cursor + 4);
            cursor += 8;
            if (length % 4 || length > bytes.size() - cursor)
                return invalid("GLB chunk alignment/range is invalid");
            if (chunk == 0)
            {
                if (type != 0x4e4f534a || length == 0)
                    return invalid("First GLB chunk must be JSON");
                json.assign(reinterpret_cast<const char*>(bytes.data() + cursor), length);
            }
            else if (type == 0x4e4f534a)
                return invalid("Duplicate GLB JSON chunk");
            else if (type == 0x004e4942)
            {
                if (chunk != 1 || hasBin)
                    return invalid("GLB BIN must be the second chunk and occur once");
                hasBin = true;
                bin.assign(bytes.begin() + cursor, bytes.begin() + cursor + length);
            }
            else if (warnings)
                warnings->push_back("Unknown GLB chunk ignored");
            cursor += length;
            ++chunk;
        }
        if (containsBin)
            *containsBin = hasBin;
        return true;
    }
} // namespace gts::gltf
