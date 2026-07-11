#pragma once

#include "ECSWorld.hpp"
#include "GeometryBindingLifecycle.h"
#include "IResourceProvider.hpp"
#include "MeshGpuComponent.h"
#include "RenderDirtyComponent.h"
#include "RenderGpuComponent.h"
#include "WorldTransformComponent.h"

namespace gts::rendering
{
    inline bool renderObjectReady(ECSWorld& world, Entity entity)
    {
        return hasRenderableDescriptor(world, entity)
            && meshGpuReady(world, entity)
            && materialGpuReady(world, entity)
            && world.hasComponent<WorldTransformComponent>(entity);
    }

    inline void syncRenderObjectLifecycle(ECSWorld& world,
                                          Entity entity,
                                          IResourceProvider* resources,
                                          ECSWorld::EntityCommandBuffer& commands)
    {
        if (resources == nullptr)
            return;

        if (!renderObjectReady(world, entity))
        {
            scheduleRenderObjectCleanup(world, commands, entity);
            return;
        }

        const bool hasRenderGpu = world.hasComponent<RenderGpuComponent>(entity);
        const bool hasDirty = world.hasComponent<RenderDirtyComponent>(entity);
        RenderGpuComponent renderGpu = hasRenderGpu
            ? world.getComponent<RenderGpuComponent>(entity)
            : RenderGpuComponent{};

        bool changed = false;
        if (renderGpu.objectSSBOSlot == RENDERABLE_SLOT_UNALLOCATED)
        {
            renderGpu.objectSSBOSlot = resources->requestObjectSlot();
            changed = true;
        }

        if (!hasRenderGpu)
            commands.addComponent<RenderGpuComponent>(entity, renderGpu);
        else if (changed)
            world.getComponent<RenderGpuComponent>(entity) = renderGpu;

        if (!hasDirty)
            commands.addComponent<RenderDirtyComponent>(entity, RenderDirtyComponent{});

        if (changed || !hasDirty)
            queueRenderSnapshotDirty(world, entity);
    }
}
