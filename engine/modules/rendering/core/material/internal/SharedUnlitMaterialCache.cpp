#include "SharedUnlitMaterialCache.h"

#include <functional>
#include "ECSWorld.hpp"

namespace gts::rendering
{
    size_t UnlitMaterialDescriptorHash::operator()(const UnlitMaterialDescriptor& descriptor) const noexcept
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
    void UnlitMaterialDescriptorHash::combine(size_t& seed, size_t value) noexcept
    {
        seed ^= value + 0x9e3779b97f4a7c15ull + (seed << 6u) + (seed >> 2u);
    }
    std::unordered_map<ECSWorld*, SharedUnlitMaterialCache>& sharedUnlitMaterialCacheRegistry(ECSWorld* world)
    {
        // Static worlds may outlive this storage during process shutdown.
        static bool alive = true;
        struct Registry : std::unordered_map<ECSWorld*, SharedUnlitMaterialCache>
        {
            ~Registry()
            {
                alive = false;
            }
        };
        static Registry registry;
        if (world != nullptr && !registry.contains(world))
        {
            world->registerTeardownCallback(
                [](ECSWorld& retiringWorld) noexcept
                {
                    if (alive)
                        registry.erase(&retiringWorld);
                });
        }
        return registry;
    }

    SharedUnlitMaterialCache& sharedUnlitMaterialCache(ECSWorld& world)
    {
        auto& registry = sharedUnlitMaterialCacheRegistry();
        if (auto it = registry.find(&world); it != registry.end())
            return it->second;

        return sharedUnlitMaterialCacheRegistry(&world)[&world];
    }

    void resetSharedUnlitMaterialCache(ECSWorld& world)
    {
        sharedUnlitMaterialCacheRegistry().erase(&world);
    }

} // namespace gts::rendering
