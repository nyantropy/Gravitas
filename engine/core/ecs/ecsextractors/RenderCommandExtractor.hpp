#pragma once

#include <vulkan/vulkan.h>
#include <array>

#include "RenderResourceManager.hpp"
#include "ECSWorld.hpp"
#include "MeshComponent.h"
#include "MaterialComponent.h"
#include "TransformComponent.h"
#include "UniformBufferComponent.h"

#include "RenderCommand.h"

// keep this close to the ecs - that means no vulkan calls here
class RenderCommandExtractor
{
    public:
        // entirely data based approach
        std::vector<RenderCommand> extractRenderList(ECSWorld& world) 
        {
            std::vector<RenderCommand> cmds;
            for (Entity e : world.getAllEntitiesWith<MeshComponent, MaterialComponent, UniformBufferComponent>()) 
            {
                auto& meshComp = world.getComponent<MeshComponent>(e);
                auto& matComp = world.getComponent<MaterialComponent>(e);
                auto& uboComp = world.getComponent<UniformBufferComponent>(e);

                cmds.push_back({
                    meshComp.meshID,
                    matComp.textureID,
                    uboComp.uniformID,
                    &uboComp.uniformBufferObject
                });
            }
            return cmds;
        }
};