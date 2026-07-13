#pragma once

#include <filesystem>
#include <string>

#include "AssetSerializers.h"

namespace gts::rendering
{
    class ModelAssetLoader
    {
    public:
        static bool load(const std::filesystem::path& path,
                         ModelAssetData& asset,
                         std::string* error = nullptr)
        {
            return ModelAssetSerializer::readFile(path, asset, error);
        }
    };
}
