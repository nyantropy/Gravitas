#pragma once

#include <cstdint>
#include <vector>
#include <vulkan/vulkan.h>

struct FrameOutputBeginResult
{
    uint32_t imageIndex = 0;
    VkSemaphore waitSemaphore = VK_NULL_HANDLE;
};

class IFrameOutputTarget
{
    public:
        virtual ~IFrameOutputTarget() = default;

        virtual FrameOutputBeginResult beginFrame(uint32_t currentFrame, VkSemaphore imageAvailableSemaphore) = 0;
        virtual void endFrame(uint32_t imageIndex, VkSemaphore renderFinishedSemaphore) = 0;

        virtual const std::vector<VkImage>& getImages() const = 0;
        virtual const std::vector<VkImageView>& getImageViews() const = 0;
        virtual VkFormat getFormat() const = 0;
        virtual VkExtent2D getExtent() const = 0;
        virtual VkPresentModeKHR getPresentMode() const = 0;
        virtual bool requiresRenderFinishedSemaphore() const = 0;
        virtual VkImageLayout getSceneFinalLayout() const = 0;
        virtual VkImageLayout getUiInitialLayout() const = 0;
        virtual VkImageLayout getUiFinalLayout() const = 0;
};
