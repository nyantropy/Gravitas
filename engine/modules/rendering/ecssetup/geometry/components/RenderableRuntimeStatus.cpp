#include "RenderableRuntimeStatus.h"

#include "ECSWorld.hpp"
#include "MaterialReferenceComponent.h"
#include "MaterialRuntimeAccess.h"
#include "MaterialRuntimeRegistry.h"
#include "MeshGpuComponent.h"
#include "RenderGpuComponent.h"

namespace gts::rendering
{
    RenderableRuntimeStatus renderableRuntimeStatus(const ECSWorld& world, Entity entity)
    {
        RenderableRuntimeStatus status;

        if (world.hasComponent<MeshGpuComponent>(entity))
        {
            const MeshGpuComponent& meshGpu = world.getComponent<MeshGpuComponent>(entity);
            status.meshGpuReady             = meshGpu.meshID != 0;
        }

        if (world.hasComponent<MaterialReferenceComponent>(entity))
        {
            const MaterialReferenceComponent& material = world.getComponent<MaterialReferenceComponent>(entity);
            const auto*                       runtime  = findMaterialRuntime(world);
            status.materialGpuReady =
                runtime != nullptr && MaterialRuntimeAccess::getGpuState(*runtime, material.material) != nullptr;
        }

        if (world.hasComponent<RenderGpuComponent>(entity))
        {
            const RenderGpuComponent& renderGpu = world.getComponent<RenderGpuComponent>(entity);
            status.renderObjectAllocated        = renderGpu.objectSSBOSlot != RENDERABLE_SLOT_UNALLOCATED;
            status.renderReady                  = renderGpu.readyToRender;
        }

        return status;
    }

} // namespace gts::rendering
