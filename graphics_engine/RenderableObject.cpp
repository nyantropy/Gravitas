#include "RenderableObject.h"

#include<string.h>
#include<iostream>

void RenderableObject::destroy(VkDevice& device, int frames)
{
    for (int i = 0; i < frames; i++) 
    {
        //uniform buffers
        vkDestroyBuffer(device, this->uniformBuffers[i], nullptr);
        vkFreeMemory(device, this->uniformBuffersMemory[i], nullptr);
    }

    //texture and imageview stuff
    vkDestroyImageView(device, this->textureImageView, nullptr);
    vkDestroyImage(device, this->textureImage, nullptr);
    vkFreeMemory(device, this->textureImageMemory, nullptr);

    //vertex buffer
    vkDestroyBuffer(device, this->vertexBuffer, nullptr);
    vkFreeMemory(device, this->vertexBufferMemory, nullptr);
}

void RenderableObject::draw(VkCommandBuffer& commandBuffer, VkPipelineLayout& pipelineLayout, uint32_t currentFrame)
{
    VkBuffer vertexBuffers[] = {vertexBuffer};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

    vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);

    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSets[currentFrame], 0, nullptr);

    vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);
}

glm::mat4 RenderableObject::calculateLocalModelMatrix()
{
    return this->localTranslationMatrix * this->localRotationMatrix * this->localScaleMatrix;
}

glm::vec3 RenderableObject::getXYZ()
{
    //Extracting the XYZ coordinates from the translation matrix
    glm::vec4 translation_column = this->currentModelMatrix[3];
    return glm::vec3(translation_column);
}

void RenderableObject::updateUniforms(GtsCamera& camera, int frames)
{
    //std::cout << "Updating Uniforms!" << std::endl;
    UniformBufferObject ubo{};
    ubo.view = camera.getViewMatrix();
    ubo.proj = camera.getProjectionMatrix();
    ubo.model = this->currentModelMatrix;

    for(int i = 0; i < frames; i++)
    { 
        memcpy(this->uniformBuffersMapped[i], &ubo, sizeof(ubo));
    }
}
