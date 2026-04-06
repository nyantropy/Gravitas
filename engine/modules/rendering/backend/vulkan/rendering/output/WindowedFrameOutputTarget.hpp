#pragma once

#include <stdexcept>

#include "IFrameOutputTarget.hpp"
#include "vcsheet.h"

class WindowedFrameOutputTarget : public IFrameOutputTarget
{
    public:
        FrameOutputBeginResult beginFrame(uint32_t currentFrame, VkSemaphore imageAvailableSemaphore) override
        {
            (void)currentFrame;

            uint32_t imageIndex = 0;
            VkResult result = vkAcquireNextImageKHR(
                vcsheet::getDevice(),
                vcsheet::getSwapChain(),
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

        void endFrame(uint32_t imageIndex, VkSemaphore renderFinishedSemaphore) override
        {
            VkPresentInfoKHR presentInfo{};
            presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
            presentInfo.waitSemaphoreCount = 1;
            presentInfo.pWaitSemaphores = &renderFinishedSemaphore;

            VkSwapchainKHR swapChains[] = { vcsheet::getSwapChain() };
            presentInfo.swapchainCount = 1;
            presentInfo.pSwapchains = swapChains;
            presentInfo.pImageIndices = &imageIndex;

            VkResult result = vkQueuePresentKHR(vcsheet::getPresentQueue(), &presentInfo);
            if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR && result != VK_ERROR_OUT_OF_DATE_KHR)
                throw std::runtime_error("failed to present swap chain image!");
        }

        const std::vector<VkImage>& getImages() const override { return vcsheet::getSwapChainImages(); }
        const std::vector<VkImageView>& getImageViews() const override { return vcsheet::getSwapChainImageViews(); }
        VkFormat getFormat() const override { return vcsheet::getSwapChainImageFormat(); }
        VkExtent2D getExtent() const override { return vcsheet::getSwapChainExtent(); }
        VkPresentModeKHR getPresentMode() const override { return vcsheet::getPresentMode(); }
        bool requiresRenderFinishedSemaphore() const override { return true; }
        VkImageLayout getSceneFinalLayout() const override { return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR; }
        VkImageLayout getUiInitialLayout() const override { return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR; }
        VkImageLayout getUiFinalLayout() const override { return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR; }
};
