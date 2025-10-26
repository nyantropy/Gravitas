#include "FrameSyncObjects.hpp"

FrameSyncObjects::FrameSyncObjects()
{
    createSyncObjects();
}

FrameSyncObjects::~FrameSyncObjects()
{
    for (VkFence fence : inFlightFences) 
    {
        if (fence != VK_NULL_HANDLE)
            vkDestroyFence(vcsheet::getDevice(), fence, nullptr);
    }

    for (VkSemaphore sem : imageAvailableSemaphores) 
    {
        if (sem != VK_NULL_HANDLE)
            vkDestroySemaphore(vcsheet::getDevice(), sem, nullptr);
    }

    for (VkSemaphore sem : renderFinishedSemaphores) 
    {
        if (sem != VK_NULL_HANDLE)
            vkDestroySemaphore(vcsheet::getDevice(), sem, nullptr);
    }
}

void FrameSyncObjects::createSyncObjects()
{
    size_t swapchainImageCount = vcsheet::getSwapChainImages().size();

    imageAvailableSemaphores.resize(GraphicsConstants::MAX_FRAMES_IN_FLIGHT);
    renderFinishedSemaphores.resize(swapchainImageCount);
    inFlightFences.resize(GraphicsConstants::MAX_FRAMES_IN_FLIGHT);

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    // create per-frame semaphores & fences
    for (size_t i = 0; i < GraphicsConstants::MAX_FRAMES_IN_FLIGHT; i++) 
    {
        if (vkCreateSemaphore(vcsheet::getDevice(), &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateFence(vcsheet::getDevice(), &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create per-frame sync objects!");
        }
    }

    // create per-swapchain-image render-finished semaphores
    for (size_t i = 0; i < swapchainImageCount; i++) 
    {
        if (vkCreateSemaphore(vcsheet::getDevice(), &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create per-swapchain-image renderFinishedSemaphore!");
        }
    }
}
