#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <variant>
#include <vector>

// the actual image in bytes
struct GtsModelEmbeddedImage
{
    std::vector<uint8_t> bytes;
    std::string mimeType;
};

// container for the texture
struct GtsModelImage
{
    std::string name;
    std::variant<std::filesystem::path, GtsModelEmbeddedImage> source;
};
