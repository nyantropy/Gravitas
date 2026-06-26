#pragma once

#include "ECSWorld.hpp"
#include "EngineToolStateComponent.h"

namespace gts::tools
{
    inline bool engineToolsVisible(ECSWorld& world)
    {
        return world.hasAny<EngineToolStateComponent>()
            && world.getSingleton<EngineToolStateComponent>().visible;
    }
}
