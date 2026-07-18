#pragma once

#include <limits>

#include "Entity.h"
#include "Types.h"

// Optional render-view camera selection override.
//
// Scene cameras keep their authored active state. Editor and tooling code can
// publish a preferred camera here when a view should render from a non-scene
// camera without mutating gameplay camera components.
struct RenderCameraSelectionComponent
{
    Entity preferredCamera{std::numeric_limits<entity_id_type>::max()};
};
