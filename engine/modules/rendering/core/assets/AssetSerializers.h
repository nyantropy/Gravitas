#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "AssetTypes.h"

namespace gts::rendering
{
    enum class CookedAssetType : uint16_t
    {
        Mesh = 1,
        Material = 2,
        Model = 3
    };

    inline constexpr uint32_t CookedAssetMagic = 0x54534147u; // GAST, little-endian.
    inline constexpr uint16_t MeshAssetFormatVersion = 1;
    inline constexpr uint16_t MaterialAssetFormatVersion = 1;
    inline constexpr uint16_t ModelAssetFormatVersion = 1;
    inline constexpr uint32_t CookedAssetHeaderSize = 64;

    inline constexpr size_t CookedAssetMagicOffset = 0;
    inline constexpr size_t CookedAssetVersionOffset = 4;
    inline constexpr size_t CookedAssetTypeOffset = 6;
    inline constexpr size_t CookedAssetPayloadSizeOffset = 32;
    inline constexpr size_t CookedAssetDependencyTableOffsetOffset = 40;
    inline constexpr size_t CookedAssetDependencyTableSizeOffset = 48;

    inline constexpr uint32_t MaxCookedMeshVertices = 4'000'000u;
    inline constexpr uint32_t MaxCookedMeshIndices = 24'000'000u;
    inline constexpr uint32_t MaxCookedMeshSubmeshes = 1'000'000u;
    inline constexpr uint32_t MaxCookedModelNodes = 1'000'000u;
    inline constexpr uint32_t MaxCookedAssetDependencies = 1'000'000u;
    inline constexpr uint32_t MaxCookedAssetStringBytes = 1'048'576u;

    class MeshAssetSerializer
    {
    public:
        static bool serialize(const MeshAssetData& asset,
                              std::vector<uint8_t>& bytes,
                              std::string* error = nullptr);

        static bool deserialize(const std::vector<uint8_t>& bytes,
                                MeshAssetData& asset,
                                std::string* error = nullptr);

        static bool writeFile(const MeshAssetData& asset,
                              const std::filesystem::path& path,
                              std::string* error = nullptr);

        static bool readFile(const std::filesystem::path& path,
                             MeshAssetData& asset,
                             std::string* error = nullptr);
    };

    class MaterialAssetSerializer
    {
    public:
        static bool serialize(const MaterialAssetData& asset,
                              std::vector<uint8_t>& bytes,
                              std::string* error = nullptr);

        static bool deserialize(const std::vector<uint8_t>& bytes,
                                MaterialAssetData& asset,
                                std::string* error = nullptr);

        static bool writeFile(const MaterialAssetData& asset,
                              const std::filesystem::path& path,
                              std::string* error = nullptr);

        static bool readFile(const std::filesystem::path& path,
                             MaterialAssetData& asset,
                             std::string* error = nullptr);
    };

    class ModelAssetSerializer
    {
    public:
        static bool serialize(const ModelAssetData& asset,
                              std::vector<uint8_t>& bytes,
                              std::string* error = nullptr);

        static bool deserialize(const std::vector<uint8_t>& bytes,
                                ModelAssetData& asset,
                                std::string* error = nullptr);

        static bool writeFile(const ModelAssetData& asset,
                              const std::filesystem::path& path,
                              std::string* error = nullptr);

        static bool readFile(const std::filesystem::path& path,
                             ModelAssetData& asset,
                             std::string* error = nullptr);
    };
}
