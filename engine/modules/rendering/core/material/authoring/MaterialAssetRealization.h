#pragma once

#include <filesystem>
#include <string>

#include "AssetMaterialTypes.h"
#include "MaterialRuntime.h"

namespace gts::rendering
{
    struct MaterialAssetData;
    struct AssetReference;
    class MaterialAssetRealization
    {
        public:
        static MaterialInstance makeInstance(const MaterialAssetData&     asset,
                                             MaterialDefinitionHandle     definition,
                                             const std::filesystem::path& baseDirectory = {});

        static MaterialInstanceHandle
        loadIntoRuntime(const std::filesystem::path& path, MaterialRuntime& runtime, std::string* error = nullptr);

        static MaterialInstanceHandle
        createInstance(MaterialInstance instance, MaterialShaderFamily family, MaterialRuntime& runtime);

        private:
        static MaterialTextureBinding textureBinding(const AssetReference&        reference,
                                                     TextureColorSpace            colorSpace,
                                                     const std::filesystem::path& baseDirectory);
    };
} // namespace gts::rendering
