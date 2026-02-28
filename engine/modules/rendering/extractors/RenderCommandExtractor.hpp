#pragma once

#include <vector>

#include "ECSWorld.hpp"
#include "RenderGpuComponent.h"
#include "CameraGpuComponent.h"
#include "RenderCommand.h"

// Reads only RenderGpuComponent (per-object render state) and the active
// CameraGpuComponent.  No Vulkan types; no pointers into ECS memory.
//
// Camera selection is driven by CameraGpuComponent::active, which is written
// by both CameraGpuSystem (default cameras) and custom override systems such
// as TetrisCameraSystem.  No dependency on CameraDescriptionComponent.
class RenderCommandExtractor
{
public:
    std::vector<RenderCommand> extractRenderList(ECSWorld& world)
    {
        // find the active camera's backend state
        view_id_type cameraViewID  = 0;
        glm::mat4    viewMatrix    = glm::mat4(1.0f);
        glm::mat4    projMatrix    = glm::mat4(1.0f);

        for (Entity e : world.getAllEntitiesWith<CameraGpuComponent>())
        {
            auto& gpu = world.getComponent<CameraGpuComponent>(e);

            if (!gpu.active)
                continue;

            cameraViewID = gpu.viewID;
            viewMatrix   = gpu.viewMatrix;
            projMatrix   = gpu.projMatrix;
            break;
        }

        // one command per renderable entity that has a bound SSBO slot
        std::vector<RenderCommand> cmds;

        for (Entity e : world.getAllEntitiesWith<RenderGpuComponent>())
        {
            auto& rc = world.getComponent<RenderGpuComponent>(e);

            if (rc.objectSSBOSlot == RENDERABLE_SLOT_UNALLOCATED)
                continue; // slot not yet assigned by RenderBindingSystem — skip this frame

            cmds.push_back({
                rc.meshID,
                rc.textureID,
                rc.objectSSBOSlot,
                cameraViewID,
                rc.modelMatrix,
                viewMatrix,
                projMatrix
            });
        }

        return cmds;
    }
};
