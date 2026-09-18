#pragma once

#include <filesystem>
#include <string>

#include "assets/loading/cooked/MaterialAssetLoader.h"
#include "MaterialRuntime.h"

namespace gts::rendering
{
    class MaterialAssetRealization
    {
    public:
        static MaterialInstance makeInstance(const MaterialAssetData& asset,
                                             MaterialDefinitionHandle definition,
                                             const std::filesystem::path& baseDirectory = {})
        {
            MaterialInstance instance;
            instance.definition = definition;
            instance.baseColor = asset.baseColor;
            instance.baseColorTexture =
                textureBinding(asset.baseColorTexture, TextureColorSpace::SRgb, baseDirectory);
            instance.metallicRoughnessTexture =
                textureBinding(asset.metallicRoughnessTexture, TextureColorSpace::Linear, baseDirectory);
            instance.normalTexture =
                textureBinding(asset.normalTexture, TextureColorSpace::Linear, baseDirectory);
            instance.ambientOcclusionTexture =
                textureBinding(asset.ambientOcclusionTexture, TextureColorSpace::Linear, baseDirectory);
            instance.emissiveTexture =
                textureBinding(asset.emissiveTexture, TextureColorSpace::SRgb, baseDirectory);
            instance.metallic = asset.metallic;
            instance.roughness = asset.roughness;
            instance.normalScale = asset.normalScale;
            instance.ambientOcclusionStrength = asset.ambientOcclusionStrength;
            instance.emissiveFactor = asset.emissiveFactor;
            instance.emissiveStrength = asset.emissiveStrength;
            instance.renderState = asset.renderState;
            instance.vertexColorOnly = asset.vertexColorOnly;
            return instance;
        }

        static MaterialInstanceHandle loadIntoRuntime(const std::filesystem::path& path,
                                                      MaterialRuntime& runtime,
                                                      std::string* error = nullptr)
        {
            MaterialAssetData asset;
            if (!MaterialAssetLoader::load(path, asset, error))
                return {};

            MaterialDefinition definition;
            definition.shaderFamily = asset.shaderFamily;
            const MaterialDefinitionHandle definitionHandle = runtime.createDefinition(definition);
            return runtime.createInstance(makeInstance(asset, definitionHandle, path.parent_path()));
        }

    private:
        static MaterialTextureBinding textureBinding(const AssetReference& reference,
                                                     TextureColorSpace colorSpace,
                                                     const std::filesystem::path& baseDirectory)
        {
            if (reference.logicalPath.empty())
                return MaterialTextureBinding::assetPath({}, colorSpace);

            std::filesystem::path texturePath(reference.logicalPath);
            if (!texturePath.is_absolute() && !baseDirectory.empty())
                texturePath = baseDirectory / texturePath;
            return MaterialTextureBinding::assetPath(texturePath.lexically_normal().generic_string(), colorSpace);
        }
    };
}
