#pragma once

#include <vector>

#include "ECSWorld.hpp"
#include "RenderableComponent.h"
#include "CameraComponent.h"
#include "CameraGpuComponent.h"
#include "RenderCommand.h"

// Reads only RenderableComponent (per-object render intent) and the active
// camera GPU component.  No longer depends on MeshComponent, MaterialComponent,
// or ObjectGpuComponent.
class RenderCommandExtractor
{
public:
    std::vector<RenderCommand> extractRenderList(ECSWorld& world)
    {
        // find the active camera
        view_id_type cameraViewID = 0;
        CameraUBO* cameraUboPtr = nullptr;

        for (Entity e : world.getAllEntitiesWith<CameraComponent, CameraGpuComponent>())
        {
            auto& camComp = world.getComponent<CameraComponent>(e);
            if (!camComp.active)
                continue;

            auto& camGpu = world.getComponent<CameraGpuComponent>(e);
            cameraViewID = camGpu.viewID;
            cameraUboPtr = &camGpu.ubo;
            break;
        }

        // one command per renderable entity that has a bound SSBO slot
        std::vector<RenderCommand> cmds;

        for (Entity e : world.getAllEntitiesWith<RenderableComponent>())
        {
            auto& rc = world.getComponent<RenderableComponent>(e);

            if (rc.objectSSBOSlot == RENDERABLE_SLOT_UNALLOCATED)
                continue; // slot not yet assigned by RenderBindingSystem — skip this frame

            cmds.push_back({
                rc.meshID,
                rc.textureID,
                rc.objectSSBOSlot,
                cameraViewID,
                rc.modelMatrix,
                cameraUboPtr
            });
        }

        return cmds;
    }
};
