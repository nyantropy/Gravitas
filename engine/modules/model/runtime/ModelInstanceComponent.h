#pragma once
#include <memory>
#include "model/runtime/GtsModelInstance.h"

// Stable occurrence ownership across ECS archetype copies. No derived render state.
struct ModelInstanceComponent
{
    std::shared_ptr<GtsModelInstance> instance;
};
