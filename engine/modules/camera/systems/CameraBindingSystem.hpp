#pragma once

#include <cstdio>

#include "ECSControllerSystem.hpp"
#include "CameraDescriptionComponent.h"
#include "CameraGpuComponent.h"

// Controller system — GPU resource management only, no matrix math.
// Mirrors the mesh binding systems for the camera pipeline.
// Matrix calculations can be found in the CameraGpuSystem
class CameraBindingSystem : public ECSControllerSystem
{
    public:
        void update(ECSWorld& world, SceneContext& ctx) override
        {
            size_t cameraEntityCount = 0;
            world.forEach<CameraDescriptionComponent>([&](Entity e, CameraDescriptionComponent&)
            {
                cameraEntityCount += 1;

                if (!world.hasComponent<CameraGpuComponent>(e))
                    world.addComponent(e, CameraGpuComponent{});

                auto& gpu = world.getComponent<CameraGpuComponent>(e);

                if (gpu.viewID == 0)
                    gpu.viewID = ctx.resources->requestCameraBuffer();

                if (gpu.dirty)
                {
                    ctx.resources->uploadCameraView(gpu.viewID, gpu.viewMatrix, gpu.projMatrix);
                    gpu.dirty = false;
                }
            });

            static uint64_t frameCounter = 0;
            frameCounter += 1;
            if ((frameCounter % 120u) == 0u)
            {
                std::printf("[camera] binding activeCameraEntities=%zu\n", cameraEntityCount);
            }
        }
};
