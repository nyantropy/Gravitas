#pragma once

#include <cstdint>
#include <limits>

#include "Entity.h"

namespace gts::tools
{
    struct EngineToolSelectionHighlightComponent
    {
        Entity highlightEntity{std::numeric_limits<entity_id_type>::max()};
        Entity targetEntity{std::numeric_limits<entity_id_type>::max()};
        uint64_t geometryVersion = 0;
    };
}
