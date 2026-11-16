#include "MaterialAssetRealization.h"

#include "MaterialAssetLoader.h"

namespace gts::rendering
{
    MaterialInstance MaterialAssetRealization::makeInstance(const MaterialAssetData&     asset,
                                                            MaterialDefinitionHandle     definition,
                                                            const std::filesystem::path& baseDirectory)
    {
        MaterialInstance instance;
        instance.definition       = definition;
        instance.baseColor        = asset.baseColor;
        instance.baseColorTexture = textureBinding(asset.baseColorTexture, TextureColorSpace::SRgb, baseDirectory);
        instance.metallicRoughnessTexture =
            textureBinding(asset.metallicRoughnessTexture, TextureColorSpace::Linear, baseDirectory);
        instance.normalTexture = textureBinding(asset.normalTexture, TextureColorSpace::Linear, baseDirectory);
        instance.ambientOcclusionTexture =
            textureBinding(asset.ambientOcclusionTexture, TextureColorSpace::Linear, baseDirectory);
        instance.emissiveTexture = textureBinding(asset.emissiveTexture, TextureColorSpace::SRgb, baseDirectory);
        instance.metallic        = asset.metallic;
        instance.roughness       = asset.roughness;
        instance.normalScale     = asset.normalScale;
        instance.ambientOcclusionStrength = asset.ambientOcclusionStrength;
        instance.emissiveFactor           = asset.emissiveFactor;
        instance.emissiveStrength         = asset.emissiveStrength;
        instance.renderState              = asset.renderState;
        instance.vertexColorOnly          = asset.vertexColorOnly;
        return instance;
    }

    MaterialInstanceHandle MaterialAssetRealization::loadIntoRuntime(const std::filesystem::path& path,
                                                                     MaterialRuntime&             runtime,
                                                                     std::string*                 error)
    {
        MaterialAssetData asset;
        if (!MaterialAssetLoader::load(path, asset, error))
            return {};

        return createInstance(makeInstance(asset, {}, path.parent_path()), asset.shaderFamily, runtime);
    }

    MaterialInstanceHandle MaterialAssetRealization::createInstance(MaterialInstance     instance,
                                                                    MaterialShaderFamily family,
                                                                    MaterialRuntime&     runtime)
    {
        MaterialDefinition definition;
        definition.shaderFamily = family;
        instance.definition     = runtime.createDefinition(definition);
        try
        {
            return runtime.createInstance(instance);
        }
        catch (...)
        {
            runtime.destroyDefinition(instance.definition);
            throw;
        }
    }

    MaterialTextureBinding MaterialAssetRealization::textureBinding(const AssetReference&        reference,
                                                                    TextureColorSpace            colorSpace,
                                                                    const std::filesystem::path& baseDirectory)
    {
        if (reference.logicalPath.empty())
            return MaterialTextureBinding::assetPath({}, colorSpace);

        std::filesystem::path texturePath(reference.logicalPath);
        if (!texturePath.is_absolute() && !baseDirectory.empty())
            texturePath = baseDirectory / texturePath;
        return MaterialTextureBinding::assetPath(texturePath.lexically_normal().generic_string(), colorSpace);
    }
} // namespace gts::rendering
