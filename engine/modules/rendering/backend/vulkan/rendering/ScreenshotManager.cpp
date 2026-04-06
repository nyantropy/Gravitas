#include "ScreenshotManager.hpp"

#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <vector>

#include <stb_image_write.h>

#include "BufferUtil.hpp"
#include "vcsheet.h"

uint32_t ScreenshotManager::bytesPerPixelForFormat(VkFormat format)
{
    switch (format)
    {
        case VK_FORMAT_B8G8R8A8_SRGB:
        case VK_FORMAT_B8G8R8A8_UNORM:
        case VK_FORMAT_R8G8B8A8_SRGB:
        case VK_FORMAT_R8G8B8A8_UNORM:
            return 4;

        case VK_FORMAT_B8G8R8_SRGB:
        case VK_FORMAT_B8G8R8_UNORM:
        case VK_FORMAT_R8G8B8_SRGB:
        case VK_FORMAT_R8G8B8_UNORM:
            return 3;

        default:
            throw std::runtime_error("unsupported swapchain format for screenshot capture");
    }
}

bool ScreenshotManager::formatIsBgr(VkFormat format)
{
    switch (format)
    {
        case VK_FORMAT_B8G8R8A8_SRGB:
        case VK_FORMAT_B8G8R8A8_UNORM:
        case VK_FORMAT_B8G8R8_SRGB:
        case VK_FORMAT_B8G8R8_UNORM:
            return true;

        default:
            return false;
    }
}

std::string ScreenshotManager::allocateScreenshotPath()
{
    namespace fs = std::filesystem;

    const fs::path directory = fs::path(GRAVITAS_ENGINE_ROOT) / "screenshots";
    fs::create_directories(directory);

    for (uint32_t index = 0; index < 10000; ++index)
    {
        std::ostringstream name;
        name << "screenshot_" << std::setw(4) << std::setfill('0') << index << ".png";
        fs::path candidate = directory / name.str();
        if (!fs::exists(candidate))
            return candidate.string();
    }

    throw std::runtime_error("could not allocate screenshot filename");
}

void ScreenshotManager::saveSwapchainImage(uint32_t imageIndex) const
{
    VkDevice device = vcsheet::getDevice();
    vkDeviceWaitIdle(device);

    const VkExtent2D extent = vcsheet::getSwapChainExtent();
    const VkFormat format = vcsheet::getSwapChainImageFormat();
    const uint32_t bytesPerPixel = bytesPerPixelForFormat(format);
    const VkDeviceSize sourceSize =
        static_cast<VkDeviceSize>(extent.width) * extent.height * bytesPerPixel;

    VkBuffer stagingBuffer = VK_NULL_HANDLE;
    VkDeviceMemory stagingBufferMemory = VK_NULL_HANDLE;
    BufferUtil::createBuffer(
        device,
        vcsheet::getPhysicalDevice(),
        sourceSize,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        stagingBuffer,
        stagingBufferMemory);

    try
    {
        VkCommandBuffer commandBuffer =
            BufferUtil::beginSingleTimeCommands(device, vcsheet::getCommandPool());

        VkImageMemoryBarrier toTransferBarrier{};
        toTransferBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        toTransferBarrier.oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        toTransferBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        toTransferBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toTransferBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toTransferBarrier.image = vcsheet::getSwapChainImages()[imageIndex];
        toTransferBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        toTransferBarrier.subresourceRange.baseMipLevel = 0;
        toTransferBarrier.subresourceRange.levelCount = 1;
        toTransferBarrier.subresourceRange.baseArrayLayer = 0;
        toTransferBarrier.subresourceRange.layerCount = 1;
        toTransferBarrier.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
        toTransferBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

        vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            0,
            0, nullptr,
            0, nullptr,
            1, &toTransferBarrier);

        VkBufferImageCopy copyRegion{};
        copyRegion.bufferOffset = 0;
        copyRegion.bufferRowLength = 0;
        copyRegion.bufferImageHeight = 0;
        copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copyRegion.imageSubresource.mipLevel = 0;
        copyRegion.imageSubresource.baseArrayLayer = 0;
        copyRegion.imageSubresource.layerCount = 1;
        copyRegion.imageOffset = {0, 0, 0};
        copyRegion.imageExtent = {extent.width, extent.height, 1};

        vkCmdCopyImageToBuffer(
            commandBuffer,
            vcsheet::getSwapChainImages()[imageIndex],
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            stagingBuffer,
            1,
            &copyRegion);

        VkImageMemoryBarrier toPresentBarrier = toTransferBarrier;
        toPresentBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        toPresentBarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        toPresentBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        toPresentBarrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;

        vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            0,
            0, nullptr,
            0, nullptr,
            1, &toPresentBarrier);

        BufferUtil::endSingleTimeCommands(
            device,
            vcsheet::getGraphicsQueue(),
            vcsheet::getCommandPool(),
            commandBuffer);

        void* mappedData = nullptr;
        vkMapMemory(device, stagingBufferMemory, 0, sourceSize, 0, &mappedData);

        const auto* srcBytes = static_cast<const std::uint8_t*>(mappedData);
        std::vector<std::uint8_t> rgbaPixels(
            static_cast<size_t>(extent.width) * extent.height * 4u);

        const bool bgrSource = formatIsBgr(format);
        for (uint32_t y = 0; y < extent.height; ++y)
        {
            const auto* srcRow =
                srcBytes + static_cast<size_t>(y) * extent.width * bytesPerPixel;
            auto* dstRow =
                rgbaPixels.data() + static_cast<size_t>(y) * extent.width * 4u;

            for (uint32_t x = 0; x < extent.width; ++x)
            {
                const auto* srcPixel = srcRow + static_cast<size_t>(x) * bytesPerPixel;
                auto* dstPixel = dstRow + static_cast<size_t>(x) * 4u;

                if (bgrSource)
                {
                    dstPixel[0] = srcPixel[2];
                    dstPixel[1] = srcPixel[1];
                    dstPixel[2] = srcPixel[0];
                }
                else
                {
                    dstPixel[0] = srcPixel[0];
                    dstPixel[1] = srcPixel[1];
                    dstPixel[2] = srcPixel[2];
                }

                dstPixel[3] = 255;
            }
        }

        vkUnmapMemory(device, stagingBufferMemory);

        const std::string outputPath = allocateScreenshotPath();
        // we dont need to flip it, its already upside down before stb ever sees it
        stbi_flip_vertically_on_write(false);
        const int writeOk = stbi_write_png(
            outputPath.c_str(),
            static_cast<int>(extent.width),
            static_cast<int>(extent.height),
            4,
            rgbaPixels.data(),
            static_cast<int>(extent.width * 4));

        if (writeOk == 0)
            throw std::runtime_error("failed to write screenshot png");

        std::cout << "Saved screenshot: " << outputPath << std::endl;
    }
    catch (...)
    {
        if (stagingBuffer != VK_NULL_HANDLE)
            vkDestroyBuffer(device, stagingBuffer, nullptr);
        if (stagingBufferMemory != VK_NULL_HANDLE)
            vkFreeMemory(device, stagingBufferMemory, nullptr);
        throw;
    }

    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingBufferMemory, nullptr);
}
