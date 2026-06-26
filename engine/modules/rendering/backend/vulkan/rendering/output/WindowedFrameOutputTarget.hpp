#pragma once

#include <stdexcept>

#include "IFrameOutputTarget.hpp"
#include "VulkanBackendContext.h"

class WindowedFrameOutputTarget : public IFrameOutputTarget
{
    public:
        explicit WindowedFrameOutputTarget(VulkanBackendContext& backendContext)
            : backendContext(backendContext)
        {
        }

        FrameOutputBeginResult beginFrame(uint32_t currentFrame, VkSemaphore imageAvailableSemaphore) override
        {
            (void)currentFrame;

            uint32_t imageIndex = 0;
            VkResult result = vkAcquireNextImageKHR(
                backendContext.device(),
                backendContext.swapChain(),
                UINT64_MAX,
                imageAvailableSemaphore,
                VK_NULL_HANDLE,
                &imageIndex);

            if (result == VK_ERROR_OUT_OF_DATE_KHR)
                return {.imageIndex = UINT32_MAX, .waitSemaphore = VK_NULL_HANDLE};

            if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
                throw std::runtime_error("failed to acquire swap chain image!");

            return {.imageIndex = imageIndex, .waitSemaphore = imageAvailableSemaphore};
        }

        bool endFrame(uint32_t imageIndex, VkSemaphore renderFinishedSemaphore) override
        {
            VkPresentInfoKHR presentInfo{};
            presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
            presentInfo.waitSemaphoreCount = 1;
            presentInfo.pWaitSemaphores = &renderFinishedSemaphore;

            VkSwapchainKHR swapChains[] = { backendContext.swapChain() };
            presentInfo.swapchainCount = 1;
            presentInfo.pSwapchains = swapChains;
            presentInfo.pImageIndices = &imageIndex;

            VkResult result = vkQueuePresentKHR(backendContext.presentQueue(), &presentInfo);
            if (result == VK_SUBOPTIMAL_KHR || result == VK_ERROR_OUT_OF_DATE_KHR)
                return true;
            if (result != VK_SUCCESS)
                throw std::runtime_error("failed to present swap chain image!");
            return false;
        }

        const std::vector<VkImage>& getImages() const override { return backendContext.swapChainImages(); }
        const std::vector<VkImageView>& getImageViews() const override { return backendContext.swapChainImageViews(); }
        VkFormat getFormat() const override { return backendContext.swapChainImageFormat(); }
        VkExtent2D getExtent() const override { return backendContext.swapChainExtent(); }
        VkPresentModeKHR getPresentMode() const override { return backendContext.presentMode(); }
        bool requiresRenderFinishedSemaphore() const override { return true; }
        VkImageLayout getUiInitialLayout() const override { return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR; }
        VkImageLayout getUiFinalLayout() const override { return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR; }

    private:
        VulkanBackendContext& backendContext;
};
