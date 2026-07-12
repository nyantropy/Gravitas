#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>

#include "ECSWorld.hpp"
#include "GlmConfig.h"
#include "MaterialReferenceComponent.h"
#include "MaterialRuntime.h"

namespace gts::rendering
{
    struct UnlitMaterialDescriptor
    {
        std::string texturePath;
        glm::vec4 baseColor = {1.0f, 1.0f, 1.0f, 1.0f};
        MaterialBlendMode blendMode = MaterialBlendMode::Alpha;
        bool doubleSided = false;
        bool vertexColorOnly = false;
        bool depthWrite = true;
    };

    inline bool operator==(const UnlitMaterialDescriptor& lhs,
                           const UnlitMaterialDescriptor& rhs)
    {
        return lhs.texturePath == rhs.texturePath
            && lhs.baseColor == rhs.baseColor
            && lhs.blendMode == rhs.blendMode
            && lhs.doubleSided == rhs.doubleSided
            && lhs.vertexColorOnly == rhs.vertexColorOnly
            && lhs.depthWrite == rhs.depthWrite;
    }

    struct UnlitMaterialDescriptorHash
    {
        size_t operator()(const UnlitMaterialDescriptor& descriptor) const noexcept
        {
            size_t seed = std::hash<std::string>{}(descriptor.texturePath);
            combine(seed, std::hash<float>{}(descriptor.baseColor.r));
            combine(seed, std::hash<float>{}(descriptor.baseColor.g));
            combine(seed, std::hash<float>{}(descriptor.baseColor.b));
            combine(seed, std::hash<float>{}(descriptor.baseColor.a));
            combine(seed, std::hash<uint32_t>{}(static_cast<uint32_t>(descriptor.blendMode)));
            combine(seed, std::hash<bool>{}(descriptor.doubleSided));
            combine(seed, std::hash<bool>{}(descriptor.vertexColorOnly));
            combine(seed, std::hash<bool>{}(descriptor.depthWrite));
            return seed;
        }

        static void combine(size_t& seed, size_t value) noexcept
        {
            seed ^= value + 0x9e3779b97f4a7c15ull + (seed << 6u) + (seed >> 2u);
        }
    };

    using SharedUnlitMaterialCache =
        std::unordered_map<UnlitMaterialDescriptor,
                           MaterialInstanceHandle,
                           UnlitMaterialDescriptorHash>;

    inline auto& sharedUnlitMaterialCacheRegistry()
    {
        static std::unordered_map<ECSWorld*, SharedUnlitMaterialCache> registry;
        return registry;
    }

    inline void resetSharedUnlitMaterialCache(ECSWorld& world)
    {
        sharedUnlitMaterialCacheRegistry().erase(&world);
    }

    inline MaterialInstance makeUnlitMaterialInstance(MaterialRuntime& runtime,
                                                      const UnlitMaterialDescriptor& descriptor)
    {
        MaterialInstance instance;
        if (const MaterialInstance* defaultMaterial = runtime.getInstance(runtime.defaultMaterial()))
            instance.definition = defaultMaterial->definition;
        instance.baseColor = descriptor.baseColor;
        if (!descriptor.texturePath.empty())
            instance.baseColorTexture = MaterialTextureBinding::assetPath(descriptor.texturePath);
        instance.vertexColorOnly = descriptor.vertexColorOnly;
        instance.renderState.doubleSided = descriptor.doubleSided;
        instance.renderState.depthWrite = descriptor.depthWrite;
        instance.renderState.blendMode = descriptor.blendMode;
        instance.renderState.alphaMode =
            alphaModeForBlendMode(
                descriptor.blendMode,
                descriptor.baseColor.a,
                descriptor.depthWrite);
        return instance;
    }

    inline MaterialInstanceHandle createUnlitMaterial(ECSWorld& world,
                                                      const UnlitMaterialDescriptor& descriptor)
    {
        MaterialRuntime& runtime = materialRuntime(world);
        return runtime.createInstance(makeUnlitMaterialInstance(runtime, descriptor));
    }

    inline MaterialInstanceHandle getOrCreateSharedUnlitMaterial(
        ECSWorld& world,
        const UnlitMaterialDescriptor& descriptor)
    {
        MaterialRuntime& runtime = materialRuntime(world);
        SharedUnlitMaterialCache& cache = sharedUnlitMaterialCacheRegistry()[&world];
        auto it = cache.find(descriptor);
        if (it != cache.end() && runtime.isInstanceAlive(it->second))
            return it->second;

        const MaterialInstanceHandle handle =
            runtime.createInstance(makeUnlitMaterialInstance(runtime, descriptor));
        cache[descriptor] = handle;
        return handle;
    }

    inline MaterialReferenceComponent sharedUnlitMaterialReference(
        ECSWorld& world,
        const UnlitMaterialDescriptor& descriptor)
    {
        return MaterialReferenceComponent{getOrCreateSharedUnlitMaterial(world, descriptor)};
    }
}
