#pragma once
#include <vulkan/vulkan.h>
#include <stdexcept>

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
            ImageUtil::createImage(this->config.vkDevice, this->config.vkExtent.width, this->config.vkExtent.height,
            config.format, config.tiling, config.imageUsageFlags, this->image);

            // then we allocate memory on the device
            MemoryUtil::allocateImageMemory(this->config.vkDevice, this->config.vkPhysicalDevice,
            this->image, this->config.memoryPropertyFlags, this->memory);

            // and finally, we create a fitting ImageView
            view = ImageUtil::createImageView(this->config.vkDevice, this->image, this->config.format, this->config.imageAspectFlags);
        }

        ~Attachment()
        {
            vkDestroyImageView(this->config.vkDevice, view, nullptr);
            vkDestroyImage(this->config.vkDevice, image, nullptr);
            vkFreeMemory(this->config.vkDevice, memory, nullptr);
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
