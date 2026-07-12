#pragma once

#include <filesystem>
#include <string>

#include "AssetSerializers.h"
#include "MaterialRuntime.h"

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

        static MaterialInstance makeInstance(const MaterialAssetData& asset,
                                             MaterialDefinitionHandle definition)
        {
            MaterialInstance instance;
            instance.definition = definition;
            instance.baseColor = asset.baseColor;
            instance.baseColorTexture = textureBinding(asset.baseColorTexture, TextureColorSpace::SRgb);
            instance.metallicRoughnessTexture =
                textureBinding(asset.metallicRoughnessTexture, TextureColorSpace::Linear);
            instance.normalTexture = textureBinding(asset.normalTexture, TextureColorSpace::Linear);
            instance.ambientOcclusionTexture =
                textureBinding(asset.ambientOcclusionTexture, TextureColorSpace::Linear);
            instance.emissiveTexture = textureBinding(asset.emissiveTexture, TextureColorSpace::SRgb);
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
            if (!load(path, asset, error))
                return {};

            MaterialDefinition definition;
            definition.shaderFamily = asset.shaderFamily;
            const MaterialDefinitionHandle definitionHandle = runtime.createDefinition(definition);
            return runtime.createInstance(makeInstance(asset, definitionHandle));
        }

    private:
        static MaterialTextureBinding textureBinding(const AssetReference& reference,
                                                     TextureColorSpace colorSpace)
        {
            return MaterialTextureBinding::assetPath(reference.logicalPath, colorSpace);
        }
    };
}
