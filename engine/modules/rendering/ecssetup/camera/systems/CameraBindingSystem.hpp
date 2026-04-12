#pragma once

#include "ECSControllerSystem.hpp"
#include "CameraDescriptionComponent.h"
#include "CameraGpuComponent.h"

// Controller system — GPU resource management only, no matrix math.
// Mirrors the mesh binding systems for the camera pipeline.
// Matrix calculations can be found in the CameraGpuSystem
class CameraBindingSystem : public ECSControllerSystem
{
    public:
        void update(const EcsControllerContext& ctx) override
        {
            ctx.world.forEach<CameraDescriptionComponent>([&](Entity e, CameraDescriptionComponent&)
            {
                if (!ctx.world.hasComponent<CameraGpuComponent>(e))
                    ctx.world.addComponent(e, CameraGpuComponent{});

                auto& gpu = ctx.world.getComponent<CameraGpuComponent>(e);

                if (gpu.viewID == 0)
                    gpu.viewID = ctx.resources->requestCameraBuffer();

                if (gpu.dirty)
                {
                    ctx.resources->uploadCameraView(gpu.viewID, gpu.viewMatrix, gpu.projMatrix);
                    gpu.dirty = false;
                }
            });
        }
};
