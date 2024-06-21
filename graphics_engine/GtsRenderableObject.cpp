#include "GtsRenderableObject.hpp"

GtsRenderableObject::GtsRenderableObject(VulkanLogicalDevice* vlogicaldevice, VulkanPhysicalDevice* vphysicaldevice, VulkanRenderer* vrenderer,
 std::string model_path, std::string texture_path, int frames_in_flight)
{
    this->frames_in_flight = frames_in_flight;
    this->vlogicaldevice = vlogicaldevice;

    //probably inefficient to load a model multiple times, but it should be fine for a small game
    GtsModelLoader::loadModel(model_path, vertices, indices);
    objecttexture = new VulkanTexture(vlogicaldevice, vphysicaldevice, vrenderer, texture_path);
    GtsBufferService::createVertexBuffer(vlogicaldevice, vphysicaldevice, vertices, vertexBuffer, vertexBufferMemory);
    GtsBufferService::createIndexBuffer(vlogicaldevice, vphysicaldevice, indices, indexBuffer, indexBufferMemory);
    GtsBufferService::createUniformBuffers(vlogicaldevice, vphysicaldevice, uniformBuffers, uniformBuffersMemory, uniformBuffersMapped, frames_in_flight);
}

GtsRenderableObject::~GtsRenderableObject()
{
    for (int i = 0; i < frames_in_flight; i++) 
    {
        //uniform buffers
        vkDestroyBuffer(vlogicaldevice->getDevice(), uniformBuffers[i], nullptr);
        vkFreeMemory(vlogicaldevice->getDevice(), uniformBuffersMemory[i], nullptr);
    }

    // //vertex buffer
    vkDestroyBuffer(vlogicaldevice->getDevice(), vertexBuffer, nullptr);
    vkFreeMemory(vlogicaldevice->getDevice(), vertexBufferMemory, nullptr);

    // //index buffer
    vkDestroyBuffer(vlogicaldevice->getDevice(), indexBuffer, nullptr);
    vkFreeMemory(vlogicaldevice->getDevice(), indexBufferMemory, nullptr);

    delete objecttexture;
}

std::vector<VkBuffer>& GtsRenderableObject::getUniformBuffers()
{
    return uniformBuffers;
}

std::vector<void*>& GtsRenderableObject::getUniformBuffersMapped()
{
    return uniformBuffersMapped;
}

std::vector<VkDescriptorSet>& GtsRenderableObject::getDescriptorSets()
{
    return descriptorSets;
}

VulkanTexture& GtsRenderableObject::getTexture()
{
    return *objecttexture;
}

void GtsRenderableObject::draw(VkCommandBuffer& commandBuffer, VkPipelineLayout& pipelineLayout, uint32_t currentFrame)
{
    VkBuffer vertexBuffers[] = {vertexBuffer};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

    vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);

    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSets[currentFrame], 0, nullptr);

    vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);
}

glm::mat4 GtsRenderableObject::calculateLocalModelMatrix()
{
    return this->localTranslationMatrix * this->localRotationMatrix * this->localScaleMatrix;
}

glm::vec3 GtsRenderableObject::getXYZ()
{
    //Extracting the XYZ coordinates from the translation matrix
    glm::vec4 translation_column = this->currentModelMatrix[3];
    return glm::vec3(translation_column);
}

void GtsRenderableObject::updateUniforms(GtsCamera& camera, int frames_in_flight)
{
    //std::cout << "Updating Uniforms!" << std::endl;
    UniformBufferObject ubo{};
    ubo.view = camera.getViewMatrix();
    ubo.proj = camera.getProjectionMatrix();
    ubo.model = this->currentModelMatrix;

    for(int i = 0; i < frames_in_flight; i++)
    { 
        memcpy(this->uniformBuffersMapped[i], &ubo, sizeof(ubo));
    }
}
