#pragma once

#include <vulkan/vulkan.h>

#include "ECSWorld.hpp"
#include "MaterialComponent.h"
#include "TransformComponent.h"
#include "MeshComponent.h"

class RenderSystem 
{
public:
    void update(ECSWorld& world, VkCommandBuffer cmd, VkPipelineLayout layout, uint32_t frameIndex) 
    {
        // we dont need an offset for the vertex buffer
        VkDeviceSize offsets[] = {0};

        for (Entity e : world.getAllEntitiesWith<MeshComponent, MaterialComponent, TransformComponent>()) 
        {
            auto& mesh = world.getComponent<MeshComponent>(e);
            auto& mat = world.getComponent<MaterialComponent>(e);
            auto& transform = world.getComponent<TransformComponent>(e);

            vkCmdBindVertexBuffers(cmd, 0, 1, &mesh.vertexBuffer, offsets);
            vkCmdBindIndexBuffer(cmd, mesh.indexBuffer, 0, VK_INDEX_TYPE_UINT32);
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, 1, &mat.descriptorSets[frameIndex], 0, nullptr);
            vkCmdDrawIndexed(cmd, static_cast<uint32_t>(mesh.indices.size()), 1, 0, 0, 0);
        }
    }
};