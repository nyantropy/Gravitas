#include "MaterialReferenceHelpers.h"

#include <filesystem>
#include <stdexcept>
#include "MaterialAssetRealization.h"
#include "MaterialRuntime.h"
#include "SharedUnlitMaterialCache.h"

namespace gts::rendering
{
    bool operator==(const UnlitMaterialDescriptor& lhs, const UnlitMaterialDescriptor& rhs)
    {
        return lhs.texturePath == rhs.texturePath && lhs.baseColor == rhs.baseColor && lhs.blendMode == rhs.blendMode &&
               lhs.doubleSided == rhs.doubleSided && lhs.vertexColorOnly == rhs.vertexColorOnly &&
               lhs.depthWrite == rhs.depthWrite;
    }

    namespace
    {
        bool isCookedMaterialPath(const std::string& path)
        {
            return std::filesystem::path(path).extension() == ".gmat";
        }

        MaterialInstanceHandle loadCookedMaterialReference(MaterialRuntime& runtime, const std::string& path)
        {
            std::string                  error;
            const MaterialInstanceHandle handle = MaterialAssetRealization::loadIntoRuntime(path, runtime, &error);
            if (!handle.valid())
                throw std::runtime_error("Failed to load cooked material asset '" + path + "': " + error);
            return handle;
        }

        MaterialInstance makeUnlitMaterialInstance(MaterialRuntime& runtime, const UnlitMaterialDescriptor& descriptor)
        {
            MaterialInstance instance;
            if (const MaterialInstance* defaultMaterial = runtime.getInstance(runtime.defaultMaterial()))
                instance.definition = defaultMaterial->definition;
            instance.baseColor = descriptor.baseColor;
            if (!descriptor.texturePath.empty())
                instance.baseColorTexture = MaterialTextureBinding::assetPath(descriptor.texturePath);
            instance.vertexColorOnly         = descriptor.vertexColorOnly;
            instance.renderState.doubleSided = descriptor.doubleSided;
            instance.renderState.depthWrite  = descriptor.depthWrite;
            instance.renderState.blendMode   = descriptor.blendMode;
            instance.renderState.alphaMode =
                alphaModeForBlendMode(descriptor.blendMode, descriptor.baseColor.a, descriptor.depthWrite);
            return instance;
        }

    } // namespace
    MaterialInstanceHandle createUnlitMaterial(ECSWorld& world, const UnlitMaterialDescriptor& descriptor)
    {
        MaterialRuntime& runtime = materialRuntime(world);
        if (isCookedMaterialPath(descriptor.texturePath))
            return loadCookedMaterialReference(runtime, descriptor.texturePath);

        return runtime.createInstance(makeUnlitMaterialInstance(runtime, descriptor));
    }

    MaterialInstanceHandle getOrCreateSharedUnlitMaterial(ECSWorld& world, const UnlitMaterialDescriptor& descriptor)
    {
        MaterialRuntime&          runtime = materialRuntime(world);
        SharedUnlitMaterialCache& cache   = sharedUnlitMaterialCache(world);
        auto                      it      = cache.find(descriptor);
        if (it != cache.end() && runtime.isInstanceAlive(it->second))
            return it->second;

        const MaterialInstanceHandle handle =
            isCookedMaterialPath(descriptor.texturePath)
                ? loadCookedMaterialReference(runtime, descriptor.texturePath)
                : runtime.createInstance(makeUnlitMaterialInstance(runtime, descriptor));
        cache[descriptor] = handle;
        return handle;
    }

    MaterialInstanceHandle getOrCreateSharedCookedMaterial(ECSWorld& world, const std::string& path)
    {
        return getOrCreateSharedUnlitMaterial(world, UnlitMaterialDescriptor{path});
    }

    MaterialReferenceComponent sharedUnlitMaterialReference(ECSWorld& world, const UnlitMaterialDescriptor& descriptor)
    {
        return MaterialReferenceComponent{getOrCreateSharedUnlitMaterial(world, descriptor)};
    }

} // namespace gts::rendering
