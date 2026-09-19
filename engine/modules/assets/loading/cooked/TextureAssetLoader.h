#pragma once

#include <filesystem>
#include <string>

#include "AssetSerializers.h"

namespace gts::rendering
{
    class TextureAssetLoader
    {
    public:
        static bool load(const std::filesystem::path& path,
                         TextureAssetData& asset,
                         std::string* error = nullptr)
        {
            return TextureAssetSerializer::readFile(path, asset, error);
        }
    };
}
