#pragma once

#include <vector>

#include "ECSControllerSystem.hpp"
#include "RenderResourceClearComponent.h"
#include "RenderGpuComponent.h"

// clears render resources if needed, the exact opposite of the binding system
class RenderResourceClearSystem : public ECSControllerSystem
{
    public:
        void update(ECSWorld& world, SceneContext& ctx) override
        {
            std::vector<Entity> toDestroy;

            world.forEach<RenderResourceClearComponent>([&](Entity e, RenderResourceClearComponent& clear)
            {
                if (world.hasComponent<RenderGpuComponent>(e))
                {
                    auto& rc = world.getComponent<RenderGpuComponent>(e);
                    if (rc.objectSSBOSlot != RENDERABLE_SLOT_UNALLOCATED)
                        ctx.resources->releaseObjectSlot(rc.objectSSBOSlot);
                }

                if (clear.destroyAfterClear)
                    toDestroy.push_back(e);
                else
                    world.removeComponent<RenderResourceClearComponent>(e);
            });

            for (Entity e : toDestroy)
                world.destroyEntity(e);
        }
};
