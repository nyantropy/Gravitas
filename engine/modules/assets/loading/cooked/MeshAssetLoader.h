#pragma once

#include <filesystem>
#include <string>

#include "AssetSerializers.h"

namespace gts::rendering
{
    class MeshAssetLoader
    {
    public:
        static bool load(const std::filesystem::path& path,
                         MeshAssetData& asset,
                         std::string* error = nullptr)
        {
            return MeshAssetSerializer::readFile(path, asset, error);
        }
    };
}
