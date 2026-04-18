#pragma once

#include "ECSControllerSystem.hpp"
#include "CameraDescriptionComponent.h"
#include "CameraGpuComponent.h"

// Controller system — GPU resource management only, no matrix math.
// Matrix calculations can be found in the CameraGpuSystem.
// CameraGpuComponent must already exist when this system runs.
class CameraBindingSystem : public ECSControllerSystem
{
    public:
        void update(const EcsControllerContext& ctx) override
        {
            ctx.world.forEach<CameraDescriptionComponent, CameraGpuComponent>(
                [&](Entity e, CameraDescriptionComponent&, CameraGpuComponent& gpu)
            {
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
