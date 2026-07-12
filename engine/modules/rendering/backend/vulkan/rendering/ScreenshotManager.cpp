#include "ScreenshotManager.hpp"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <future>
#include <iomanip>
#include <ios>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <utility>
#include <vector>

#include <stb_image_write.h>

#include "BufferUtil.hpp"
#include "GtsPaths.h"

namespace
{
    struct ScreenshotWriteJob
    {
        std::string outputPath;
        std::vector<std::uint8_t> sourcePixels;
        uint32_t width = 0;
        uint32_t height = 0;
        uint32_t bytesPerPixel = 0;
        bool bgrSource = false;
    };

    std::vector<std::uint8_t> convertToRgba(const ScreenshotWriteJob& job)
    {
        std::vector<std::uint8_t> rgbaPixels(
            static_cast<size_t>(job.width) * job.height * 4u);

        for (uint32_t y = 0; y < job.height; ++y)
        {
            const auto* srcRow =
                job.sourcePixels.data() + static_cast<size_t>(y) * job.width * job.bytesPerPixel;
            auto* dstRow =
                rgbaPixels.data() + static_cast<size_t>(y) * job.width * 4u;

            for (uint32_t x = 0; x < job.width; ++x)
            {
                const auto* srcPixel = srcRow + static_cast<size_t>(x) * job.bytesPerPixel;
                auto* dstPixel = dstRow + static_cast<size_t>(x) * 4u;

                if (job.bgrSource)
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

        return rgbaPixels;
    }

    void writeScreenshotPng(ScreenshotWriteJob job)
    {
        try
        {
            const std::vector<std::uint8_t> rgbaPixels = convertToRgba(job);
            const int writeOk = stbi_write_png(
                job.outputPath.c_str(),
                static_cast<int>(job.width),
                static_cast<int>(job.height),
                4,
                rgbaPixels.data(),
                static_cast<int>(job.width * 4u));

            if (writeOk == 0)
                throw std::runtime_error("failed to write screenshot png");

            std::cout << "Saved screenshot: " << job.outputPath << std::endl;
        }
        catch (const std::exception& e)
        {
            std::cerr << "Screenshot write failed: " << e.what() << std::endl;
        }
        catch (...)
        {
            std::cerr << "Screenshot write failed: unknown error" << std::endl;
        }
    }
} // namespace

ScreenshotManager::~ScreenshotManager()
{
    try
    {
        waitForPendingWork();
    }
    catch (const std::exception& e)
    {
        std::cerr << "Screenshot shutdown failed: " << e.what() << std::endl;
    }
    catch (...)
    {
        std::cerr << "Screenshot shutdown failed: unknown error" << std::endl;
    }
    destroyCaptureSlots();
}

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
            throw std::runtime_error("unsupported frame output format for screenshot capture");
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

std::string ScreenshotManager::allocateScreenshotPath(const std::string& outputDirectory)
{
    namespace fs = std::filesystem;

    fs::path directory = outputDirectory.empty()
        ? GtsPaths::GetProjectRoot() / "screenshots"
        : fs::path(outputDirectory);
    if (directory.is_relative())
        directory = GtsPaths::GetProjectRoot() / directory;
    fs::create_directories(directory);

    for (uint32_t index = 0; index < 10000; ++index)
    {
        std::ostringstream name;
        name << "screenshot_" << std::setw(4) << std::setfill('0') << index << ".png";
        fs::path candidate = directory / name.str();
        if (!fs::exists(candidate))
        {
            std::ofstream reserve(candidate, std::ios::binary | std::ios::app);
            if (!reserve)
                throw std::runtime_error("could not reserve screenshot filename");
            return candidate.string();
        }
    }

    throw std::runtime_error("could not allocate screenshot filename");
}

void ScreenshotManager::pruneCompletedWrites()
{
    for (auto it = pendingWrites.begin(); it != pendingWrites.end();)
    {
        if (it->wait_for(std::chrono::seconds(0)) == std::future_status::ready)
        {
            it->get();
            it = pendingWrites.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

void ScreenshotManager::waitForPendingWrites()
{
    for (std::future<void>& pendingWrite : pendingWrites)
    {
        if (pendingWrite.valid())
            pendingWrite.get();
    }
    pendingWrites.clear();
}

void ScreenshotManager::updatePendingStats(GtsFrameStats* stats) const
{
    if (stats == nullptr)
        return;

    uint32_t pendingGpuCaptures = 0;
    for (const CaptureSlot& slot : captureSlots)
    {
        if (slot.pendingGpu)
            ++pendingGpuCaptures;
    }

    stats->screenshotPendingGpuCount =
        std::max(stats->screenshotPendingGpuCount, pendingGpuCaptures);
    stats->screenshotPendingWriteCount =
        std::max(stats->screenshotPendingWriteCount, static_cast<uint32_t>(pendingWrites.size()));
}

ScreenshotManager::CaptureSlot* ScreenshotManager::acquireCaptureSlot()
{
    for (CaptureSlot& slot : captureSlots)
    {
        if (!slot.pendingGpu)
            return &slot;
    }

    if (captureSlots.size() >= MaxPendingCaptures)
        return nullptr;

    captureSlots.emplace_back();
    return &captureSlots.back();
}

void ScreenshotManager::destroyCaptureSlot(CaptureSlot& slot)
{
    VkDevice device = backendContext.device();
    if (slot.stagingBuffer != VK_NULL_HANDLE)
        vkDestroyBuffer(device, slot.stagingBuffer, nullptr);
    if (slot.stagingBufferMemory != VK_NULL_HANDLE)
        vkFreeMemory(device, slot.stagingBufferMemory, nullptr);
    slot = {};
}

void ScreenshotManager::destroyCaptureSlots()
{
    for (CaptureSlot& slot : captureSlots)
        destroyCaptureSlot(slot);
    captureSlots.clear();
}

void ScreenshotManager::ensureSlotCapacity(CaptureSlot& slot, VkDeviceSize requiredSize)
{
    if (slot.stagingBuffer != VK_NULL_HANDLE && slot.capacity >= requiredSize)
        return;

    destroyCaptureSlot(slot);

    VkDevice device = backendContext.device();
    BufferUtil::createBuffer(
        device,
        backendContext.physicalDevice(),
        requiredSize,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        slot.stagingBuffer,
        slot.stagingBufferMemory);
    slot.capacity = requiredSize;
}

void ScreenshotManager::recordImageCopy(VkCommandBuffer commandBuffer,
                                        VkImage image,
                                        VkBuffer stagingBuffer,
                                        VkExtent2D extent,
                                        VkImageLayout currentLayout)
{
    VkImageMemoryBarrier toTransferBarrier{};
    toTransferBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    toTransferBarrier.oldLayout = currentLayout;
    toTransferBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    toTransferBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toTransferBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toTransferBarrier.image = image;
    toTransferBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    toTransferBarrier.subresourceRange.baseMipLevel = 0;
    toTransferBarrier.subresourceRange.levelCount = 1;
    toTransferBarrier.subresourceRange.baseArrayLayer = 0;
    toTransferBarrier.subresourceRange.layerCount = 1;
    toTransferBarrier.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    toTransferBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

    vkCmdPipelineBarrier(
        commandBuffer,
        currentLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
            ? VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT
            : VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
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
        image,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        stagingBuffer,
        1,
        &copyRegion);

    VkImageMemoryBarrier toOriginalBarrier = toTransferBarrier;
    toOriginalBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    toOriginalBarrier.newLayout = currentLayout;
    toOriginalBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    toOriginalBarrier.dstAccessMask =
        currentLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
            ? VK_ACCESS_MEMORY_READ_BIT
            : VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    vkCmdPipelineBarrier(
        commandBuffer,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        currentLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
            ? VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT
            : VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        0,
        0, nullptr,
        0, nullptr,
        1, &toOriginalBarrier);
}

void ScreenshotManager::completeCapture(CaptureSlot& slot, GtsFrameStats* stats)
{
    const auto readbackStart = std::chrono::steady_clock::now();

    std::vector<std::uint8_t> sourcePixels(static_cast<size_t>(slot.byteSize));

    void* mappedData = nullptr;
    if (vkMapMemory(backendContext.device(), slot.stagingBufferMemory, 0, slot.byteSize, 0, &mappedData) != VK_SUCCESS)
        throw std::runtime_error("failed to map screenshot staging buffer");
    std::memcpy(sourcePixels.data(), mappedData, sourcePixels.size());
    vkUnmapMemory(backendContext.device(), slot.stagingBufferMemory);

    const auto readbackEnd = std::chrono::steady_clock::now();

    ScreenshotWriteJob job;
    job.outputPath = std::move(slot.outputPath);
    job.sourcePixels = std::move(sourcePixels);
    job.width = slot.extent.width;
    job.height = slot.extent.height;
    job.bytesPerPixel = slot.bytesPerPixel;
    job.bgrSource = slot.bgrSource;

    slot.pendingGpu = false;
    slot.completionFence = VK_NULL_HANDLE;
    slot.byteSize = 0;
    pendingCaptureWarningLogged = false;

    if (stats != nullptr)
    {
        stats->screenshotCompletedCount += 1;
        stats->screenshotReadbackBytes += static_cast<uint32_t>(job.sourcePixels.size());
        stats->screenshotReadbackCpuMs +=
            std::chrono::duration<float, std::milli>(readbackEnd - readbackStart).count();
    }

    pendingWrites.push_back(std::async(std::launch::async,
                                       [job = std::move(job)]() mutable
                                       {
                                           writeScreenshotPng(std::move(job));
                                       }));
}

void ScreenshotManager::pollCompletedCaptures(GtsFrameStats& stats)
{
    const auto pollStart = std::chrono::steady_clock::now();

    pruneCompletedWrites();

    for (CaptureSlot& slot : captureSlots)
    {
        if (!slot.pendingGpu || slot.completionFence == VK_NULL_HANDLE)
            continue;

        const VkResult fenceStatus = vkGetFenceStatus(backendContext.device(), slot.completionFence);
        if (fenceStatus == VK_NOT_READY)
            continue;
        if (fenceStatus != VK_SUCCESS)
            throw std::runtime_error("failed to query screenshot completion fence");

        completeCapture(slot, &stats);
    }

    updatePendingStats(&stats);
    const auto pollEnd = std::chrono::steady_clock::now();
    stats.screenshotPollCpuMs +=
        std::chrono::duration<float, std::milli>(pollEnd - pollStart).count();
}

bool ScreenshotManager::scheduleCapture(VkCommandBuffer commandBuffer,
                                        VkImage image,
                                        VkFormat format,
                                        VkExtent2D extent,
                                        VkImageLayout currentLayout,
                                        VkFence completionFence,
                                        const std::string& outputDirectory,
                                        GtsFrameStats& stats)
{
    const auto scheduleStart = std::chrono::steady_clock::now();
    stats.screenshotRequestedCount += 1;
    pruneCompletedWrites();

    const uint32_t bytesPerPixel = bytesPerPixelForFormat(format);
    const VkDeviceSize sourceSize =
        static_cast<VkDeviceSize>(extent.width) * extent.height * bytesPerPixel;

    CaptureSlot* slot = acquireCaptureSlot();
    if (slot == nullptr)
    {
        stats.screenshotSkippedCount += 1;
        if (!pendingCaptureWarningLogged)
        {
            std::cout << "Screenshot request skipped: pending screenshot capture queue is full." << std::endl;
            pendingCaptureWarningLogged = true;
        }
        updatePendingStats(&stats);
        const auto scheduleEnd = std::chrono::steady_clock::now();
        stats.screenshotScheduleCpuMs +=
            std::chrono::duration<float, std::milli>(scheduleEnd - scheduleStart).count();
        return false;
    }

    ensureSlotCapacity(*slot, sourceSize);
    slot->byteSize = sourceSize;
    slot->format = format;
    slot->extent = extent;
    slot->bytesPerPixel = bytesPerPixel;
    slot->bgrSource = formatIsBgr(format);
    slot->completionFence = completionFence;
    slot->outputPath = allocateScreenshotPath(outputDirectory);

    recordImageCopy(commandBuffer, image, slot->stagingBuffer, extent, currentLayout);

    slot->pendingGpu = true;
    stats.screenshotScheduledCount += 1;
    updatePendingStats(&stats);

    const auto scheduleEnd = std::chrono::steady_clock::now();
    stats.screenshotScheduleCpuMs +=
        std::chrono::duration<float, std::milli>(scheduleEnd - scheduleStart).count();
    return true;
}

void ScreenshotManager::waitForPendingWork()
{
    for (CaptureSlot& slot : captureSlots)
    {
        if (!slot.pendingGpu || slot.completionFence == VK_NULL_HANDLE)
            continue;

        vkWaitForFences(backendContext.device(), 1, &slot.completionFence, VK_TRUE, UINT64_MAX);
        completeCapture(slot, nullptr);
    }

    waitForPendingWrites();
}

void ScreenshotManager::cancelCapturesForFence(VkFence completionFence)
{
    if (completionFence == VK_NULL_HANDLE)
        return;

    for (CaptureSlot& slot : captureSlots)
    {
        if (!slot.pendingGpu || slot.completionFence != completionFence)
            continue;

        slot.pendingGpu = false;
        slot.completionFence = VK_NULL_HANDLE;
        slot.byteSize = 0;
        slot.outputPath.clear();
    }
}
