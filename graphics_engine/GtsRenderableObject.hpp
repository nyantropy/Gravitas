#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <string>

#include "Vertex.h"
#include "UniformBufferObject.h"
#include "GtsCamera.hpp"
#include "GtsModelLoader.hpp"
#include "GtsBufferService.hpp"
#include "VulkanTexture.hpp"
#include "VulkanRenderer.hpp"
#include "GTSDescriptorSetManager.hpp"

class GTSDescriptorSetManager;

class GtsRenderableObject
{
    public:
        int frames_in_flight;
        VulkanLogicalDevice* vlogicaldevice;
        GTSDescriptorSetManager* vdescriptorsetmanager;

        //every object has its vertex buffer
        std::vector<Vertex> vertices;
        VkBuffer vertexBuffer;
        VkDeviceMemory vertexBufferMemory;

        //and an index buffer
        std::vector<uint32_t> indices;
        VkBuffer indexBuffer;
        VkDeviceMemory indexBufferMemory;

        //and multiple uniform buffers, 1 per frame
        std::vector<VkBuffer> uniformBuffers;
        std::vector<VkDeviceMemory> uniformBuffersMemory;
        std::vector<void*> uniformBuffersMapped;

        //cant forget the descriptor sets either
        std::vector<VkDescriptorSet> descriptorSets;

        //an object also contains a texture, who would have thought
        VulkanTexture* objecttexture;
   
        GtsRenderableObject(VulkanLogicalDevice* vlogicaldevice, VulkanPhysicalDevice* vphysicaldevice, GTSDescriptorSetManager* vdescriptorsetmanager, VulkanRenderer* vrenderer,
         std::string model_path, std::string texture_path, int frames_in_flight);

        ~GtsRenderableObject();

        std::vector<VkBuffer>& getUniformBuffers();
        std::vector<void*>& getUniformBuffersMapped();
        VulkanTexture& getTexture();
        std::vector<VkDescriptorSet>& getDescriptorSets();

        void draw(VkCommandBuffer& commandBuffer, VkPipelineLayout& pipelineLayout, uint32_t currentFrame);

        void updateUniforms(const glm::mat4& modelMatrix, GtsCamera& camera, int frames_in_flight);
};