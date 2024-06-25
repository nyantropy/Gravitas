#ifndef GTS_FRAME_GRABBER_HPP
#define GTS_FRAME_GRABBER_HPP

#include <vulkan/vulkan.h>
#include <cstdint>

#include "VulkanSwapChain.hpp"
#include "VulkanRenderer.hpp"
#include "VulkanLogicalDevice.hpp"
#include "VulkanPhysicalDevice.hpp"

class GtsFrameGrabber
{
    private:
        VulkanSwapChain* vswapchain;
        VulkanRenderer* vrenderer;
        VulkanLogicalDevice* vlogicaldevice;
        VulkanPhysicalDevice* vphysicaldevice;

    public:
        GtsFrameGrabber();
        GtsFrameGrabber(VulkanSwapChain* vswapchain, VulkanRenderer* vrenderer, VulkanLogicalDevice* vlogicaldevice, VulkanPhysicalDevice* vphysicaldevice)
        {
            this->vswapchain = vswapchain;
            this->vrenderer = vrenderer;
            this->vlogicaldevice = vlogicaldevice;
            this->vphysicaldevice = vphysicaldevice;
        }

        //a single class just for this function i guess, cause i cant pass an engine pointer apparently
        void getCurrentFrame(uint32_t imageIndex, uint8_t* bufferData)
        {
            std::cout << "trying to rip a frame" << std::endl;
            //after submitting command buffer, we can pry an image from the swapchain, with a lot of effort that is
            VkImage srcImage = vswapchain->getSwapChainImages()[imageIndex];
            vrenderer->transitionImageLayout(vlogicaldevice, srcImage, vswapchain->getSwapChainImageFormat(), VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
            VkDeviceSize imageSize = vswapchain->getSwapChainExtent().width * vswapchain->getSwapChainExtent().height * 4;
            VkBuffer stagingBuffer;
            VkDeviceMemory stagingBufferMemory;
            GtsBufferService::createBuffer(vlogicaldevice, vphysicaldevice, imageSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);
            GtsBufferService::copyImageToBuffer(vlogicaldevice, srcImage, stagingBuffer, vswapchain->getSwapChainExtent().width, vswapchain->getSwapChainExtent().height);
            void* data;
            vkMapMemory(vlogicaldevice->getDevice(), stagingBufferMemory, 0, imageSize, 0, &data);
            memcpy(bufferData, data, (size_t)imageSize);

            std::cout << "VVE codebase bit" << std::endl;
            //taken from the VVE codebase - i was wondering why the picture looked weird, well i guess the format was not super correct        
            for (uint32_t i = 0; i < vswapchain->getSwapChainExtent().width * vswapchain->getSwapChainExtent().height; i++)
            {
                /*gli::byte*/ unsigned char r = bufferData[4 * i + 0];
                /*gli::byte*/ unsigned char g = bufferData[4 * i + 1];
                /*gli::byte*/ unsigned char b = bufferData[4 * i + 2];
                /*gli::byte*/ unsigned char a = bufferData[4 * i + 3];

                bufferData[4 * i + 0] = b;
                bufferData[4 * i + 1] = g;
                bufferData[4 * i + 2] = r;
                bufferData[4 * i + 3] = a;
            }

            std::cout << "trying to free resources" << std::endl;
            vrenderer->transitionImageLayout(vlogicaldevice, srcImage, vswapchain->getSwapChainImageFormat(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
            vkUnmapMemory(vlogicaldevice->getDevice(), stagingBufferMemory);
            vkDestroyBuffer(vlogicaldevice->getDevice(), stagingBuffer, nullptr);
            vkFreeMemory(vlogicaldevice->getDevice(), stagingBufferMemory, nullptr);
        }
};

#endif // GTS_FRAME_GRABBER_HPP