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

class GtsRenderableObject
{
    private:
        int frames_in_flight;
        VulkanLogicalDevice* vlogicaldevice;

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

        glm::mat4 localTranslationMatrix = glm::mat4(1.0f);
        glm::mat4 localRotationMatrix = glm::mat4(1.0f);
        glm::mat4 localScaleMatrix = glm::mat4(1.0f);

        //a finished model matrix.
        //take care though: this is not necessarily a combination of all local Transformations, but rather a variable
        //to save an already computed Model Matrix, which also takes global transformations into consideration.
        //as a concept, this is only here so we can check for coordinates and still have initial transformation access available
        glm::mat4 currentModelMatrix = glm::mat4(1.0f);

        //an object also contains a texture, who would have thought
        VulkanTexture* objecttexture;

    public:
        GtsRenderableObject(VulkanLogicalDevice* vlogicaldevice, VulkanPhysicalDevice* vphysicaldevice, VulkanRenderer* vrenderer,
         std::string model_path, std::string texture_path, int frames_in_flight);

        ~GtsRenderableObject();

        std::vector<VkBuffer>& getUniformBuffers();
        VulkanTexture& getTexture();

        glm::mat4 calculateLocalModelMatrix();

        void draw(VkCommandBuffer& commandBuffer, VkPipelineLayout& pipelineLayout, uint32_t currentFrame);

        void updateUniforms(GtsCamera& camera, int frames_in_flight);

        glm::vec3 getXYZ();
};