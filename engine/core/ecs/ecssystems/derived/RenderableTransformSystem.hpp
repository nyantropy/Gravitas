#pragma once

#include "ECSSimulationSystem.hpp"
#include "RenderableComponent.h"
#include "TransformComponent.h"

// Simulation system that keeps RenderableComponent::modelMatrix in sync with
// TransformComponent.  Replaces the old ObjectGpuDataSystem.
// Only recomputes the matrix when the dirty flag is set.
class RenderableTransformSystem : public ECSSimulationSystem
{
    public:
        void update(ECSWorld& world, float) override
        {
            world.forEach<RenderableComponent, TransformComponent>([](Entity, RenderableComponent& rc, TransformComponent& tc)
            {
                if (!rc.dirty)
                    return;

                rc.modelMatrix = tc.getModelMatrix();
                rc.dirty = false;
            });
        }
};
