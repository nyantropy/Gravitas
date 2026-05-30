#pragma once

#include <cstddef>
#include <limits>
#include <string>

#include "Entity.h"

namespace gts::tools
{
    struct EngineToolStateComponent
    {
        bool visible = false;
        size_t activePanelIndex = 0;
        Entity selectedEntity{std::numeric_limits<entity_id_type>::max()};
        std::string status = "F6 TO HIDE";
    };
}
