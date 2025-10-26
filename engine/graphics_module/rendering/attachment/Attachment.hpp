#pragma once
#include <vulkan/vulkan.h>
#include <stdexcept>
#include <iostream>

#include "vcsheet.h"
#include "AttachmentConfig.h"
#include "ImageUtil.hpp"
#include "MemoryUtil.hpp"

// a more general class to handle images
class Attachment 
{
    private:
        AttachmentConfig config;
    public:
        VkImage image = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        VkImageView view = VK_NULL_HANDLE;

        Attachment(){}

        Attachment(AttachmentConfig config)
        {
            this->config = config;

            // first of all we create an image
            ImageUtil::createImage(vcsheet::getDevice(), vcsheet::getSwapChainExtent().width, vcsheet::getSwapChainExtent().height,
            config.format, config.tiling, config.imageUsageFlags, this->image);

            // then we allocate memory on the device
            MemoryUtil::allocateImageMemory(vcsheet::getDevice(), vcsheet::getPhysicalDevice(),
            this->image, this->config.memoryPropertyFlags, this->memory);

            // and finally, we create a fitting ImageView
            view = ImageUtil::createImageView(vcsheet::getDevice(), this->image, this->config.format, this->config.imageAspectFlags);
        }

        ~Attachment()
        {
            vkDestroyImageView(vcsheet::getDevice(), view, nullptr);
            vkDestroyImage(vcsheet::getDevice(), image, nullptr);
            vkFreeMemory(vcsheet::getDevice(), memory, nullptr);
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
