#include "MaterialRuntime.h"

#include <memory>
#include <stdexcept>

int main()
{
    using namespace gts::rendering;
    ECSWorld                    independent;
    const auto                  independentToken = materialRuntime(independent).lifetimeToken();
    alignas(ECSWorld) std::byte storage[sizeof(ECSWorld)];
    for (int cycle = 0; cycle != 3; ++cycle)
    {
        auto* world = std::construct_at(reinterpret_cast<ECSWorld*>(storage));
        if (materialRuntimeRegistry().contains(world))
            throw std::runtime_error("material-only world inherited stale state");
        auto token = materialRuntime(*world).lifetimeToken();
        world->clear();
        if (!token.expired() || materialRuntimeRegistry().contains(world))
            throw std::runtime_error("material-only clear retained state");
        token = materialRuntime(*world).lifetimeToken();
        std::destroy_at(world);
        if (!token.expired() || materialRuntimeRegistry().contains(world) || independentToken.expired())
            throw std::runtime_error("material-only destruction/isolation failed");
    }
}
