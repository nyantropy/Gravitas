#pragma once

#include "ECSControllerSystem.hpp"
#include "CameraDescriptionComponent.h"
#include "CameraGpuComponent.h"

// Controller system — camera resource lifetime only, no matrix math.
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

                // Matrix uploads are performed by the renderer after the
                // current frame's fence has been waited. Clearing this keeps
                // the component's "CPU matrix changed" flag from accumulating.
                gpu.dirty = false;
            });
        }
};
