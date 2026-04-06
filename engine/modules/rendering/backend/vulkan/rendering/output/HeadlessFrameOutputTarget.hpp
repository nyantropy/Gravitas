#pragma once

#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "Attachment.hpp"
#include "AttachmentConfig.h"
#include "GraphicsConstants.h"
#include "IFrameOutputTarget.hpp"
#include "vcsheet.h"

class HeadlessFrameOutputTarget : public IFrameOutputTarget
{
    public:
        HeadlessFrameOutputTarget(uint32_t width, uint32_t height, VkFormat colorFormat)
            : extent{width, height}
            , format(colorFormat)
        {
            colorAttachments.reserve(GraphicsConstants::MAX_FRAMES_IN_FLIGHT);
            images.reserve(GraphicsConstants::MAX_FRAMES_IN_FLIGHT);
            imageViews.reserve(GraphicsConstants::MAX_FRAMES_IN_FLIGHT);

            for (uint32_t i = 0; i < GraphicsConstants::MAX_FRAMES_IN_FLIGHT; ++i)
            {
                AttachmentConfig colorConfig;
                colorConfig.width = width;
                colorConfig.height = height;
                colorConfig.format = colorFormat;
                colorConfig.tiling = VK_IMAGE_TILING_OPTIMAL;
                colorConfig.imageUsageFlags =
                    VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
                colorConfig.memoryPropertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
                colorConfig.imageAspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;

                colorAttachments.push_back(std::make_unique<Attachment>(colorConfig));
                images.push_back(colorAttachments.back()->getImage());
                imageViews.push_back(colorAttachments.back()->getImageView());
            }
        }

        FrameOutputBeginResult beginFrame(uint32_t currentFrame, VkSemaphore imageAvailableSemaphore) override
        {
            (void)imageAvailableSemaphore;
            return {.imageIndex = currentFrame % static_cast<uint32_t>(images.size()),
                    .waitSemaphore = VK_NULL_HANDLE};
        }

        void endFrame(uint32_t imageIndex, VkSemaphore renderFinishedSemaphore) override
        {
            (void)imageIndex;
            (void)renderFinishedSemaphore;
        }

        const std::vector<VkImage>& getImages() const override { return images; }
        const std::vector<VkImageView>& getImageViews() const override { return imageViews; }
        VkFormat getFormat() const override { return format; }
        VkExtent2D getExtent() const override { return extent; }
        VkPresentModeKHR getPresentMode() const override { return VK_PRESENT_MODE_IMMEDIATE_KHR; }
        bool requiresRenderFinishedSemaphore() const override { return false; }
        VkImageLayout getSceneFinalLayout() const override { return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL; }
        VkImageLayout getUiInitialLayout() const override { return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL; }
        VkImageLayout getUiFinalLayout() const override { return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL; }

    private:
        VkExtent2D extent{};
        VkFormat format = VK_FORMAT_R8G8B8A8_SRGB;
        std::vector<std::unique_ptr<Attachment>> colorAttachments;
        std::vector<VkImage> images;
        std::vector<VkImageView> imageViews;
};
