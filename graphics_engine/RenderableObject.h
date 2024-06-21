#pragma once

#include <vulkan/vulkan.h>
#include <vector>

#include "Vertex.h"
#include "UniformBufferObject.h"
#include "GtsCamera.hpp"

struct RenderableObject
{
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

    glm::mat4 localTranslationMatrix = glm::mat4(1.0f);
    glm::mat4 localRotationMatrix = glm::mat4(1.0f);
    glm::mat4 localScaleMatrix = glm::mat4(1.0f);

    //a finished model matrix.
    //take care though: this is not necessarily a combination of all local Transformations, but rather a variable
    //to save an already computed Model Matrix, which also takes global transformations into consideration.
    //as a concept, this is only here so we can check for coordinates and still have initial transformation access available
    glm::mat4 currentModelMatrix = glm::mat4(1.0f);

    //every object has a texture image and corresponding memory as well
    VkImage textureImage;
    VkDeviceMemory textureImageMemory;
    VkImageView textureImageView;

    std::vector<VkDescriptorSet> descriptorSets;

    glm::mat4 calculateLocalModelMatrix();

    void draw(VkCommandBuffer& commandBuffer, VkPipelineLayout& pipelineLayout, uint32_t currentFrame);

    void destroy(VkDevice& device, int frames_in_flight);

    void updateUniforms(GtsCamera& camera, int frames_in_flight);

    glm::vec3 getXYZ();
};