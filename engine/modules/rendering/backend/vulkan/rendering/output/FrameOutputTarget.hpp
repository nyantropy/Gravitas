#pragma once

#include <cstdint>
#include <vector>
#include <vulkan/vulkan.h>

#include "FrameManager.hpp"
#include "GtsFrameStats.h"

struct FrameOutputBeginResult
{
    bool        skipFrame = false;
    uint32_t    imageIndex = 0;
    VkSemaphore waitSemaphore = VK_NULL_HANDLE;
};

class FrameOutputTarget
{
public:
    virtual ~FrameOutputTarget() = default;

    virtual uint32_t getImageCount() const = 0;
    virtual VkExtent2D getExtent() const = 0;
    virtual VkFormat getColorFormat() const = 0;
    virtual VkImageLayout getSceneFinalLayout() const = 0;
    virtual VkImageLayout getUiInitialLayout() const = 0;
    virtual VkImageLayout getUiFinalLayout() const = 0;
    virtual const std::vector<VkImage>& getImages() const = 0;
    virtual const std::vector<VkImageView>& getImageViews() const = 0;
    virtual VkPresentModeKHR getPresentMode() const = 0;
    virtual bool isHeadless() const = 0;
    virtual void registerWithContext() = 0;

    virtual FrameOutputBeginResult beginFrame(FrameManager& frameManager,
                                              FrameResources& frame,
                                              GtsFrameStats& stats,
                                              bool& framebufferResized) = 0;

    virtual VkSemaphore getRenderFinishedSemaphore(FrameManager& frameManager,
                                                   uint32_t imageIndex) const = 0;

    virtual void endFrame(FrameManager& frameManager,
                          uint32_t imageIndex,
                          VkSemaphore renderFinishedSemaphore,
                          GtsFrameStats& stats,
                          bool framebufferResized) = 0;
};
