#pragma once
#include <vulkan/vulkan.h>
#include <stdexcept>
#include <iostream>

#include "AttachmentConfig.h"
#include "ImageUtil.hpp"
#include "MemoryUtil.hpp"
#include "VulkanBackendContext.h"

// a more general class to handle images
class Attachment 
{
    private:
        VulkanBackendContext* backendContext = nullptr;
        AttachmentConfig config;
    public:
        VkImage image = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        VkImageView view = VK_NULL_HANDLE;

        Attachment(){}

        Attachment(VulkanBackendContext& backendContext, AttachmentConfig config)
            : backendContext(&backendContext)
        {
            this->config = config;

            // first of all we create an image
            ImageUtil::createImage(backendContext.device(), config.width, config.height,
            config.format, config.tiling, config.imageUsageFlags, this->image);

            // then we allocate memory on the device
            MemoryUtil::allocateImageMemory(backendContext.device(), backendContext.physicalDevice(),
            this->image, this->config.memoryPropertyFlags, this->memory);

            // and finally, we create a fitting ImageView
            view = ImageUtil::createImageView(
                backendContext.device(), this->image, this->config.format, this->config.imageAspectFlags);
        }

        ~Attachment()
        {
            if (backendContext == nullptr)
                return;

            vkDestroyImageView(backendContext->device(), view, nullptr);
            vkDestroyImage(backendContext->device(), image, nullptr);
            vkFreeMemory(backendContext->device(), memory, nullptr);
        }

        VkImage& getImage()
        {
            return image;
        }

        VkDeviceMemory& getMemory()
        {
            return memory;
        }

        VkImageView& getImageView()
        {
            return view;
        }
};
