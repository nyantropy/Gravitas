#pragma once

#include <vector>

#include "ECSWorld.hpp"
#include "MeshComponent.h"
#include "MaterialComponent.h"
#include "CameraComponent.h"
#include "ObjectGpuComponent.h"
#include "CameraGpuComponent.h"

#include "RenderCommand.h"

// keep this close to the ecs - that means no vulkan calls here
class RenderCommandExtractor
{
    public:
        // entirely data based approach
        std::vector<RenderCommand> extractRenderList(ECSWorld& world) 
        {
            // find the active camera's GPU component
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

            std::vector<RenderCommand> cmds;
            for (Entity e : world.getAllEntitiesWith<MeshComponent, MaterialComponent, ObjectGpuComponent>())
            {
                auto& meshComp = world.getComponent<MeshComponent>(e);
                auto& matComp  = world.getComponent<MaterialComponent>(e);
                auto& objGpu   = world.getComponent<ObjectGpuComponent>(e);

                cmds.push_back({
                    meshComp.meshID,
                    matComp.textureID,
                    objGpu.objectSSBOIndex,
                    cameraViewID,
                    &objGpu.ubo,
                    cameraUboPtr
                });
            }
            return cmds;
        }
};
