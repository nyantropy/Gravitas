#include "GTSDescriptorSetManager.hpp"

GTSDescriptorSetManager::GTSDescriptorSetManager(VulkanLogicalDevice* vlogicaldevice, int frames_in_flight)
{
    this->vlogicaldevice = vlogicaldevice;
    this->frames_in_flight = frames_in_flight;

    createDescriptorSetLayout();
    createDescriptorPool();
}

GTSDescriptorSetManager::~GTSDescriptorSetManager()
{
    if(descriptorSetLayout != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorSetLayout(vlogicaldevice->getDevice(), descriptorSetLayout, nullptr);
    }

    if(descriptorPool != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorPool(vlogicaldevice->getDevice(), descriptorPool, nullptr);
    }
}

VkDescriptorSetLayout& GTSDescriptorSetManager::getDescriptorSetLayout()
{
    return descriptorSetLayout;
}

VkDescriptorPool& GTSDescriptorSetManager::getDescriptorPool()
{
    return descriptorPool;
}

void GTSDescriptorSetManager::createDescriptorSetLayout() 
{
    if(descriptorSetLayout != VK_NULL_HANDLE)
    {
        return;
    }

    VkDescriptorSetLayoutBinding uboLayoutBinding{};
    uboLayoutBinding.binding = 0;
    uboLayoutBinding.descriptorCount = 1;
    uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboLayoutBinding.pImmutableSamplers = nullptr;
    uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkDescriptorSetLayoutBinding samplerLayoutBinding{};
    samplerLayoutBinding.binding = 1;
    samplerLayoutBinding.descriptorCount = 1;
    samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    samplerLayoutBinding.pImmutableSamplers = nullptr;
    samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    std::array<VkDescriptorSetLayoutBinding, 2> bindings = {uboLayoutBinding, samplerLayoutBinding};
    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();

    if (vkCreateDescriptorSetLayout(vlogicaldevice->getDevice(), &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS) 
    {
        throw std::runtime_error("failed to create descriptor set layout!");
    }
}

void GTSDescriptorSetManager::createDescriptorPool() 
{
    std::array<VkDescriptorPoolSize, 2> poolSizes{};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = static_cast<uint32_t>(frames_in_flight);
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[1].descriptorCount = static_cast<uint32_t>(frames_in_flight);

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = static_cast<uint32_t>(frames_in_flight);

    if (vkCreateDescriptorPool(vlogicaldevice->getDevice(), &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) 
    {
        throw std::runtime_error("failed to create descriptor pool!");
    }
}

// void GTSDescriptorSetManager::createDescriptorSets() 
// {
//     std::vector<VkDescriptorSetLayout> layouts(frames_in_flight, descriptorSetLayout);
//     VkDescriptorSetAllocateInfo allocInfo{};
//     allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
//     allocInfo.descriptorPool = descriptorPool;
//     allocInfo.descriptorSetCount = static_cast<uint32_t>(frames_in_flight);
//     allocInfo.pSetLayouts = layouts.data();

//     descriptorSets.resize(frames_in_flight);
//     if (vkAllocateDescriptorSets(vlogicaldevice->getDevice(), &allocInfo, descriptorSets.data()) != VK_SUCCESS) {
//         throw std::runtime_error("failed to allocate descriptor sets!");
//     }

//     for (size_t i = 0; i < frames_in_flight; i++) 
//     {
//         VkDescriptorBufferInfo bufferInfo{};
//         bufferInfo.buffer = uniformBuffers[i];
//         bufferInfo.offset = 0;
//         bufferInfo.range = sizeof(UniformBufferObject);

//         VkDescriptorImageInfo imageInfo{};
//         imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
//         imageInfo.imageView = textureImageView;
//         imageInfo.sampler = textureSampler;

//         std::array<VkWriteDescriptorSet, 2> descriptorWrites{};

//         descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
//         descriptorWrites[0].dstSet = descriptorSets[i];
//         descriptorWrites[0].dstBinding = 0;
//         descriptorWrites[0].dstArrayElement = 0;
//         descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
//         descriptorWrites[0].descriptorCount = 1;
//         descriptorWrites[0].pBufferInfo = &bufferInfo;

//         descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
//         descriptorWrites[1].dstSet = descriptorSets[i];
//         descriptorWrites[1].dstBinding = 1;
//         descriptorWrites[1].dstArrayElement = 0;
//         descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
//         descriptorWrites[1].descriptorCount = 1;
//         descriptorWrites[1].pImageInfo = &imageInfo;

//         vkUpdateDescriptorSets(vlogicaldevice->getDevice(), static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
//     }
// }