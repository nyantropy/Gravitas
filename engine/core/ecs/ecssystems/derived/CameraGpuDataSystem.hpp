#pragma once
#include "ECSSimulationSystem.hpp"
#include "CameraComponent.h"
#include "CameraGpuComponent.h"

// a system that dynamically updates camera gpu data
class CameraGpuDataSystem : public ECSSimulationSystem
{
    public:
        void update(ECSWorld& world, float) override
        {
            world.forEach<CameraComponent, CameraGpuComponent>([&](Entity, CameraComponent& camera, CameraGpuComponent& gpu)
                {
                    if (!camera.active)
                        return;

                    if (!gpu.dirty)
                        return;

                    gpu.ubo.view = camera.view;
                    gpu.ubo.proj = camera.projection;

                    gpu.dirty = false;
                }
            );
        }
};
