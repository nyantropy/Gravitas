#include "MaterialRuntimeRegistry.h"
#include "ECSWorld.hpp"

namespace gts::rendering
{

    std::unordered_map<const ECSWorld*, MaterialRuntime>& materialRuntimeRegistry(ECSWorld* world)
    {
        // Static worlds may outlive this storage during process shutdown.
        static bool alive = true;
        struct Registry : std::unordered_map<const ECSWorld*, MaterialRuntime>
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

    MaterialRuntime& materialRuntime(ECSWorld& world)
    {
        auto& registry = materialRuntimeRegistry();
        if (auto it = registry.find(&world); it != registry.end())
            return it->second;

        return materialRuntimeRegistry(&world)[&world];
    }

    void resetMaterialRuntime(ECSWorld& world)
    {
        materialRuntimeRegistry().erase(&world);
    }

    const MaterialRuntime* findMaterialRuntime(const ECSWorld& world)
    {
        const auto& registry = materialRuntimeRegistry();
        const auto  it       = registry.find(&world);
        return it == registry.end() ? nullptr : &it->second;
    }
} // namespace gts::rendering
