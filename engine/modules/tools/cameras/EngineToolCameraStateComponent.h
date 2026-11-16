#pragma once

#include <limits>

#include "Entity.h"

namespace gts::tools
{
    struct EngineToolCameraStateComponent
    {
        bool   active = false;
        Entity toolCameraEntity{std::numeric_limits<entity_id_type>::max()};
        Entity previousActiveCamera{std::numeric_limits<entity_id_type>::max()};
    };
} // namespace gts::tools
