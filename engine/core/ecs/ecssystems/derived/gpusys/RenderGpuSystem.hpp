#pragma once

#include "ECSSimulationSystem.hpp"
#include "RenderGpuComponent.h"
#include "TransformComponent.h"

// Simulation system that keeps RenderGpuComponent::modelMatrix in sync with
// TransformComponent.  Only recomputes the matrix when the dirty flag is set.
class RenderGpuSystem : public ECSSimulationSystem
{
    public:
        void update(ECSWorld& world, float) override
        {
            world.forEach<RenderGpuComponent, TransformComponent>([](Entity, RenderGpuComponent& rc, TransformComponent& tc)
            {
                if (!rc.dirty)
                    return;

                rc.modelMatrix = tc.getModelMatrix();
                rc.dirty = false;
            });
        }
};
