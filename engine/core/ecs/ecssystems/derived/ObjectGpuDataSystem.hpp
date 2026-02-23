#pragma once
#include "ECSSimulationSystem.hpp"
#include "ObjectGpuComponent.h"
#include "TransformComponent.h"

// a system that dynamically updates object gpu data
class ObjectGpuDataSystem : public ECSSimulationSystem
{
    public:
        void update(ECSWorld& world, float) override
        {
            world.forEach<ObjectGpuComponent, TransformComponent>([&](Entity, ObjectGpuComponent& gpu, TransformComponent& transform)
                {
                    if (!gpu.dirty)
                        return;

                    gpu.ubo.model = transform.getModelMatrix();

                    gpu.dirty = false;
                }
            );
        }
};
