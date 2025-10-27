#pragma once

#include <vulkan/vulkan.h>
#include <array>

#include "ResourceSystem.hpp"
#include "ECSWorld.hpp"
#include "MeshComponent.h"
#include "MaterialComponent.h"
#include "TransformComponent.h"
#include "UniformBufferComponent.h"

#include "RenderCommand.h"

// keep this close to the ecs - that means no vulkan calls here
class RenderSystem
{
    public:
        // entirely data based approach
        std::vector<RenderCommand> buildRenderList(ECSWorld& world) 
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
                    uboComp.uniformID
                });
            }
            return cmds;
        }

        // void update(ECSWorld& world, ResourceSystem& resources, VkCommandBuffer cmd, VkPipelineLayout pipelineLayout, uint32_t frameIndex)
        // {
        //     const VkDeviceSize offsets[] = { 0 };

        //     for (Entity e : world.getAllEntitiesWith<MeshComponent, MaterialComponent, UniformBufferComponent>())
        //     {
        //         // get the three components that matter for rendering
        //         auto& meshComp = world.getComponent<MeshComponent>(e);
        //         auto& matComp = world.getComponent<MaterialComponent>(e);
        //         auto& uboComp = world.getComponent<UniformBufferComponent>(e); 

        //         vkCmdBindVertexBuffers(cmd, 0, 1, &resources.getMesh(meshComp.meshID)->vertexBuffer, offsets);
        //         vkCmdBindIndexBuffer(cmd, resources.getMesh(meshComp.meshID)->indexBuffer, 0, VK_INDEX_TYPE_UINT32);

        //         std::array<VkDescriptorSet, 2> descriptorSets = 
        //         {
        //             resources.getUniformBuffer(uboComp.uniformID)->descriptorSets[frameIndex],
        //             resources.getTexture(matComp.textureID)->descriptorSets[frameIndex]         
        //         };

        //         vkCmdBindDescriptorSets(
        //             cmd,
        //             VK_PIPELINE_BIND_POINT_GRAPHICS,
        //             pipelineLayout,
        //             0, // firstSet = 0 → because our layouts are set=0 (UBO), set=1 (texture)
        //             static_cast<uint32_t>(descriptorSets.size()),
        //             descriptorSets.data(),
        //             0,
        //             nullptr
        //         );

        //         vkCmdDrawIndexed(
        //             cmd,
        //             static_cast<uint32_t>(resources.getMesh(meshComp.meshID)->indices.size()),
        //             1,  // instance count
        //             0,  // first index
        //             0,  // vertex offset
        //             0   // first instance
        //         );
        //     }
        // }
};