#include "FrameManager.hpp"

FrameManager::FrameManager() 
{
    createFrameResources();
}

FrameManager::~FrameManager() 
{
    for (auto& frame : frames) 
    {
        if (frame.inFlightFence != VK_NULL_HANDLE)
            vkDestroyFence(vcsheet::getDevice(), frame.inFlightFence, nullptr);
        if (frame.imageAvailableSemaphore != VK_NULL_HANDLE)
            vkDestroySemaphore(vcsheet::getDevice(), frame.imageAvailableSemaphore, nullptr);
    }

    for (VkSemaphore sem : renderFinishedSemaphores) 
    {
        if (sem != VK_NULL_HANDLE)
            vkDestroySemaphore(vcsheet::getDevice(), sem, nullptr);
    }
}

void FrameManager::createFrameResources() 
{
    size_t swapchainImageCount = vcsheet::getSwapChainImages().size();

    // per-frame resources
    frames.resize(GraphicsConstants::MAX_FRAMES_IN_FLIGHT);
    for (size_t i = 0; i < frames.size(); i++) 
    {
        frames[i].commandBuffer = allocateCommandBuffer();
        frames[i].inFlightFence = createFence();
        frames[i].imageAvailableSemaphore = createSemaphore();
    }

    // per-swapchain-image renderFinished semaphores
    renderFinishedSemaphores.resize(swapchainImageCount);
    for (size_t i = 0; i < swapchainImageCount; i++) 
    {
        renderFinishedSemaphores[i] = createSemaphore();
    }

    // images in flight can be all null handles
    imagesInFlight.assign(swapchainImageCount, VK_NULL_HANDLE);
}

// create a semaphore
VkSemaphore FrameManager::createSemaphore() 
{
    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkSemaphore semaphore;
    if (vkCreateSemaphore(vcsheet::getDevice(), &semaphoreInfo, nullptr, &semaphore) != VK_SUCCESS) 
    {
        throw std::runtime_error("Failed to create semaphore!");
    }

    return semaphore;
}

// create a fence
VkFence FrameManager::createFence() 
{
    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    VkFence fence;
    if (vkCreateFence(vcsheet::getDevice(), &fenceInfo, nullptr, &fence) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create fence!");
    }

    return fence;
}

// allocate a framebuffer
VkCommandBuffer FrameManager::allocateCommandBuffer() 
{
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = vcsheet::getCommandPool();
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer cmdBuffer;
    if (vkAllocateCommandBuffers(vcsheet::getDevice(), &allocInfo, &cmdBuffer) != VK_SUCCESS) 
    {
        throw std::runtime_error("Failed to allocate command buffer!");
    }

    return cmdBuffer;
}
