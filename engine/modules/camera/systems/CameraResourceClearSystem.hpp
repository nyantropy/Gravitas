#pragma once

#include <vector>

#include "ECSControllerSystem.hpp"
#include "CameraResourceClearComponent.h"
#include "CameraGpuComponent.h"

// Releases camera GPU resources (viewID / UBO descriptor sets) for any
// camera entity marked with CameraResourceClearComponent.
// Mirrors RenderResourceClearSystem for the camera pipeline.
class CameraResourceClearSystem : public ECSControllerSystem
{
    public:
        void update(ECSWorld& world, SceneContext& ctx) override
        {
            std::vector<Entity> toDestroy;

            world.forEach<CameraResourceClearComponent>([&](Entity e, CameraResourceClearComponent& clear)
            {
                if (world.hasComponent<CameraGpuComponent>(e))
                {
                    auto& gpu = world.getComponent<CameraGpuComponent>(e);
                    if (gpu.viewID != 0)
                    {
                        ctx.resources->releaseCameraBuffer(gpu.viewID);
                        gpu.viewID = 0;
                    }
                }

                if (clear.destroyAfterClear)
                    toDestroy.push_back(e);
                else
                    world.removeComponent<CameraResourceClearComponent>(e);
            });

            for (Entity e : toDestroy)
                world.destroyEntity(e);
        }
};
