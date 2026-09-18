#pragma once

#include <filesystem>
#include <string>

#include "assets/serialization/AssetSerializers.h"

namespace gts::rendering
{
    class MaterialAssetLoader
    {
    public:
        static bool load(const std::filesystem::path& path,
                         MaterialAssetData& asset,
                         std::string* error = nullptr)
        {
            return MaterialAssetSerializer::readFile(path, asset, error);
        }
    };
}
