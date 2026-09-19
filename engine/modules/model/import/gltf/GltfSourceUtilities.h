#pragma once
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace gts::gltf
{
    bool     readFileBytes(const std::filesystem::path& path, std::vector<uint8_t>& bytes);
    uint32_t readU32LE(const std::vector<uint8_t>& bytes, size_t offset);
    bool     readGlb(const std::filesystem::path& path,
                     std::string&                 json,
                     std::vector<uint8_t>&        bin,
                     std::string&                 error,
                     bool*                        containsBin = nullptr,
                     std::vector<std::string>*    warnings    = nullptr);
    int      base64Value(char ch);
    bool     decodeBase64(std::string_view encoded, std::vector<uint8_t>& bytes);
    bool     decodeDataUri(const std::string& uri, std::string& mimeType, std::vector<uint8_t>& bytes);
    size_t   componentSize(int32_t componentType);
    size_t   componentCountForType(const std::string& type);
    float    normalizedIntegerValue(int64_t value, int32_t componentType);
} // namespace gts::gltf
