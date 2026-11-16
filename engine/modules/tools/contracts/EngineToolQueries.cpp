#include "EngineToolQueries.h"

#include "ECSWorld.hpp"
#include "EngineToolStateComponent.h"

namespace gts::tools
{
    std::optional<bool> engineToolsVisibility(const ECSWorld& world)
    {
        if (!world.hasAny<EngineToolStateComponent>())
            return std::nullopt;
        return world.getSingleton<EngineToolStateComponent>().visible;
    }
}
