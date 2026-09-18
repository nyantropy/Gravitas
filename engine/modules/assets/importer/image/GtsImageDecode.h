#pragma once
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <vector>

struct GtsDecodedImage
{
    uint32_t             width              = 0;
    uint32_t             height             = 0;
    uint32_t             sourceChannelCount = 0;
    std::vector<uint8_t> rgba8Pixels;
};
bool decodeGtsImage(const std::filesystem::path& path, GtsDecodedImage& image, std::string* error = nullptr);
bool decodeGtsImage(std::span<const uint8_t> bytes, GtsDecodedImage& image, std::string* error = nullptr);
