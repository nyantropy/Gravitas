#pragma once

#include "EcsExecutionSelection.h"

class GtsScene;
struct EcsControllerContext;

namespace gts::physics
{
    void installPhysicsFeature(GtsScene& scene,
                               EcsControllerContext& ctx,
                               const EcsExecutionSelection& defaultSelection,
                               EcsSystemGroup simulationGroup);
}
