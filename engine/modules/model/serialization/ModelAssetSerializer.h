#pragma once

#include "assets/serialization/AssetSerializers.h"
#include "model/serialization/ModelAssetTypes.h"

namespace gts::rendering
{
    inline constexpr uint16_t ModelAssetFormatVersion = 1;
    inline constexpr uint32_t MaxCookedModelNodes     = 1'000'000u;

    class ModelAssetSerializer
    {
        public:
        static bool serialize(const ModelAssetData& asset, std::vector<uint8_t>& bytes, std::string* error = nullptr);

        static bool deserialize(const std::vector<uint8_t>& bytes, ModelAssetData& asset, std::string* error = nullptr);

        static bool
        writeFile(const ModelAssetData& asset, const std::filesystem::path& path, std::string* error = nullptr);

        static bool readFile(const std::filesystem::path& path, ModelAssetData& asset, std::string* error = nullptr);
    };

} // namespace gts::rendering
