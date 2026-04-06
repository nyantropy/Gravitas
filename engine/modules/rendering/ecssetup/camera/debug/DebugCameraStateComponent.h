#pragma once

#include <limits>

#include "Entity.h"
#include "Types.h"

struct DebugCameraStateComponent
{
    bool   active = false;
    Entity debugCameraEntity    = Entity{ std::numeric_limits<entity_id_type>::max() };
    Entity previousActiveCamera = Entity{ std::numeric_limits<entity_id_type>::max() };
};
