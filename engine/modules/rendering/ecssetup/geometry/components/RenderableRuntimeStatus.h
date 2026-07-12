#pragma once

#include "ECSWorld.hpp"
#include "MaterialReferenceComponent.h"
#include "MaterialRuntime.h"
#include "MeshGpuComponent.h"
#include "RenderGpuComponent.h"

namespace gts::rendering
{
    struct RenderableRuntimeStatus
    {
        bool meshGpuReady = false;
        bool materialGpuReady = false;
        bool renderObjectAllocated = false;
        bool renderReady = false;
    };

    inline RenderableRuntimeStatus renderableRuntimeStatus(ECSWorld& world, Entity entity)
    {
        RenderableRuntimeStatus status;

        if (world.hasComponent<MeshGpuComponent>(entity))
        {
            const MeshGpuComponent& meshGpu = world.getComponent<MeshGpuComponent>(entity);
            status.meshGpuReady = meshGpu.meshID != 0;
        }

        if (world.hasComponent<MaterialReferenceComponent>(entity))
        {
            const MaterialReferenceComponent& material = world.getComponent<MaterialReferenceComponent>(entity);
            status.materialGpuReady = materialRuntime(world).getGpuState(material.material) != nullptr;
        }

        if (world.hasComponent<RenderGpuComponent>(entity))
        {
            const RenderGpuComponent& renderGpu = world.getComponent<RenderGpuComponent>(entity);
            status.renderObjectAllocated = renderGpu.objectSSBOSlot != RENDERABLE_SLOT_UNALLOCATED;
            status.renderReady = renderGpu.readyToRender;
        }

        return status;
    }
}
