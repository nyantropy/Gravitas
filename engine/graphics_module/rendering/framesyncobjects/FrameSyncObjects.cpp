#include "FrameSyncObjects.hpp"

FrameSyncObjects::FrameSyncObjects(const FrameSyncObjectsConfig& config)
    : config(config)
{
    createSyncObjects();
}

FrameSyncObjects::~FrameSyncObjects()
{
    for (size_t i = 0; i < config.maxFramesInFlight; i++) 
    {
        if (inFlightFences[i] != VK_NULL_HANDLE)
            vkDestroyFence(config.vkDevice, inFlightFences[i], nullptr);

        if (renderFinishedSemaphores[i] != VK_NULL_HANDLE)
            vkDestroySemaphore(config.vkDevice, renderFinishedSemaphores[i], nullptr);

        if (imageAvailableSemaphores[i] != VK_NULL_HANDLE)
            vkDestroySemaphore(config.vkDevice, imageAvailableSemaphores[i], nullptr);
    }
}

void FrameSyncObjects::createSyncObjects()
{
    imageAvailableSemaphores.resize(config.maxFramesInFlight);
    renderFinishedSemaphores.resize(config.maxFramesInFlight);
    inFlightFences.resize(config.maxFramesInFlight);

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (size_t i = 0; i < config.maxFramesInFlight; i++) 
    {
        if (vkCreateSemaphore(config.vkDevice, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(config.vkDevice, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS ||
            vkCreateFence(config.vkDevice, &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS) 
        {
            throw std::runtime_error("failed to create synchronization objects for a frame!");
        }
    }
}
