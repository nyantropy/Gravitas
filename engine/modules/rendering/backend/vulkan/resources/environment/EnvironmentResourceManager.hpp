#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include <stb_image.h>
#include <vulkan/vulkan.h>

#include "BufferUtil.hpp"
#include "DescriptorSetManager.hpp"
#include "GlmConfig.h"
#include "GraphicsConstants.h"
#include "LightingFrameData.h"
#include "MemoryUtil.hpp"
#include "VulkanBackendContext.h"

namespace gts::vulkan
{
    inline constexpr uint32_t EnvironmentPreprocessVersion = 1;
    inline constexpr uint32_t RadianceCubemapSize = 128;
    inline constexpr uint32_t IrradianceCubemapSize = 32;
    inline constexpr uint32_t PrefilteredSpecularCubemapSize = 128;
    inline constexpr uint32_t BrdfLutSize = 128;
    inline constexpr uint32_t IrradiancePhiSamples = 32;
    inline constexpr uint32_t IrradianceThetaSamples = 16;
    inline constexpr uint32_t PrefilterSampleCount = 64;
    inline constexpr uint32_t BrdfSampleCount = 128;
    inline constexpr VkFormat EnvironmentHdrFormat = VK_FORMAT_R32G32B32A32_SFLOAT;
    inline constexpr VkFormat EnvironmentBrdfFormat = VK_FORMAT_R32G32B32A32_SFLOAT;

    inline std::string resolveEnvironmentAssetPath(const std::string& path)
    {
        return path.rfind("/textures/", 0) == 0
            ? GraphicsConstants::ENGINE_RESOURCES + path
            : path;
    }

    inline std::string environmentCacheKeyForPath(const std::string& path)
    {
        return std::to_string(EnvironmentPreprocessVersion)
            + ":radiance=" + std::to_string(RadianceCubemapSize)
            + ":irradiance=" + std::to_string(IrradianceCubemapSize)
            + ":prefilter=" + std::to_string(PrefilteredSpecularCubemapSize)
            + ":mips=" + std::to_string(gts::rendering::EnvironmentPrefilterMipCount)
            + ":path=" + resolveEnvironmentAssetPath(path);
    }

    inline float radicalInverseVdc(uint32_t bits)
    {
        bits = (bits << 16u) | (bits >> 16u);
        bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
        bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
        bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
        bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
        return static_cast<float>(bits) * 2.3283064365386963e-10f;
    }

    inline glm::vec2 hammersley2d(uint32_t index, uint32_t count)
    {
        return {
            static_cast<float>(index) / static_cast<float>(std::max(1u, count)),
            radicalInverseVdc(index)
        };
    }

    inline glm::vec3 importanceSampleGgx(const glm::vec2& xi,
                                         const glm::vec3& normal,
                                         float roughness)
    {
        const float alpha = sanitizeMaterialRoughness(roughness);
        const float a = alpha * alpha;
        const float phi = 2.0f * gts::rendering::PbrPi * xi.x;
        const float cosTheta = std::sqrt(
            std::max(0.0f, (1.0f - xi.y) / (1.0f + (a * a - 1.0f) * xi.y)));
        const float sinTheta = std::sqrt(std::max(0.0f, 1.0f - cosTheta * cosTheta));

        const glm::vec3 halfway = {
            std::cos(phi) * sinTheta,
            std::sin(phi) * sinTheta,
            cosTheta
        };

        const glm::vec3 up =
            std::abs(normal.z) < 0.999f ? glm::vec3(0.0f, 0.0f, 1.0f) : glm::vec3(1.0f, 0.0f, 0.0f);
        const glm::vec3 tangent =
            gts::rendering::safeLightingNormalize(glm::cross(up, normal), {1.0f, 0.0f, 0.0f});
        const glm::vec3 bitangent = glm::cross(normal, tangent);
        return gts::rendering::safeLightingNormalize(
            tangent * halfway.x + bitangent * halfway.y + normal * halfway.z,
            normal);
    }

    inline glm::vec2 integrateBrdfReference(float nDotV, float roughness)
    {
        const float clampedNDotV = gts::rendering::saturateLighting(nDotV);
        const float clampedRoughness = sanitizeMaterialRoughness(roughness);
        const glm::vec3 view = {
            std::sqrt(std::max(0.0f, 1.0f - clampedNDotV * clampedNDotV)),
            0.0f,
            clampedNDotV
        };
        const glm::vec3 normal = {0.0f, 0.0f, 1.0f};

        float scale = 0.0f;
        float bias = 0.0f;
        for (uint32_t i = 0; i < BrdfSampleCount; ++i)
        {
            const glm::vec3 halfway = importanceSampleGgx(
                hammersley2d(i, BrdfSampleCount),
                normal,
                clampedRoughness);
            const glm::vec3 light = gts::rendering::safeLightingNormalize(
                2.0f * glm::dot(view, halfway) * halfway - view,
                normal);

            const float nDotL = gts::rendering::saturateLighting(light.z);
            const float nDotH = gts::rendering::saturateLighting(halfway.z);
            const float vDotH = gts::rendering::saturateLighting(glm::dot(view, halfway));
            if (nDotL <= 0.0f || nDotH <= 0.0f)
                continue;

            const float geometry = gts::rendering::geometrySmith(
                clampedNDotV,
                nDotL,
                clampedRoughness);
            const float visibility =
                (geometry * vDotH) / std::max(nDotH * clampedNDotV, gts::rendering::PbrEpsilon);
            const float fresnel = std::pow(1.0f - vDotH, 5.0f);
            scale += (1.0f - fresnel) * visibility;
            bias += fresnel * visibility;
        }

        const float invSamples = 1.0f / static_cast<float>(BrdfSampleCount);
        return {scale * invSamples, bias * invSamples};
    }

    struct HdrEquirectangularImage
    {
        uint32_t width = 0;
        uint32_t height = 0;
        std::vector<glm::vec4> pixels;

        glm::vec3 sampleDirection(const glm::vec3& direction) const
        {
            if (width == 0 || height == 0 || pixels.empty())
                return {0.0f, 0.0f, 0.0f};

            const glm::vec2 uv = gts::rendering::equirectangularUvForDirection(direction);
            const float x = uv.x * static_cast<float>(width);
            const float y = std::clamp(uv.y, 0.0f, 1.0f) * static_cast<float>(height - 1u);
            const int x0 = static_cast<int>(std::floor(x)) % static_cast<int>(width);
            const int x1 = (x0 + 1) % static_cast<int>(width);
            const int y0 = std::clamp(static_cast<int>(std::floor(y)), 0, static_cast<int>(height - 1u));
            const int y1 = std::clamp(y0 + 1, 0, static_cast<int>(height - 1u));
            const float tx = x - std::floor(x);
            const float ty = y - std::floor(y);

            const auto at = [this](int px, int py)
            {
                return glm::vec3(pixels[static_cast<size_t>(py) * width + static_cast<size_t>(px)]);
            };
            const glm::vec3 a = glm::mix(at(x0, y0), at(x1, y0), tx);
            const glm::vec3 b = glm::mix(at(x0, y1), at(x1, y1), tx);
            return glm::max(glm::mix(a, b, ty), glm::vec3(0.0f));
        }
    };

    inline HdrEquirectangularImage loadHdrEquirectangular(const std::string& path)
    {
        if (stbi_is_hdr(path.c_str()) == 0)
            throw std::runtime_error("environment source is not a Radiance HDR image: " + path);

        int width = 0;
        int height = 0;
        int channels = 0;
        float* loaded = stbi_loadf(path.c_str(), &width, &height, &channels, 4);
        if (loaded == nullptr || width <= 0 || height <= 0)
            throw std::runtime_error("failed to load HDR environment: " + path);

        std::unique_ptr<float, decltype(&stbi_image_free)> pixels(loaded, stbi_image_free);
        HdrEquirectangularImage image;
        image.width = static_cast<uint32_t>(width);
        image.height = static_cast<uint32_t>(height);
        image.pixels.resize(static_cast<size_t>(width) * static_cast<size_t>(height));
        for (size_t i = 0; i < image.pixels.size(); ++i)
        {
            image.pixels[i] = {
                std::max(0.0f, pixels.get()[i * 4 + 0]),
                std::max(0.0f, pixels.get()[i * 4 + 1]),
                std::max(0.0f, pixels.get()[i * 4 + 2]),
                1.0f
            };
        }
        return image;
    }

    inline void appendTexel(std::vector<float>& data, const glm::vec3& rgb)
    {
        data.push_back(std::max(0.0f, rgb.x));
        data.push_back(std::max(0.0f, rgb.y));
        data.push_back(std::max(0.0f, rgb.z));
        data.push_back(1.0f);
    }

    inline std::vector<float> generateRadianceCubemapData(const HdrEquirectangularImage& source,
                                                          uint32_t size)
    {
        std::vector<float> data;
        data.reserve(static_cast<size_t>(size) * size * gts::rendering::EnvironmentCubemapFaceCount * 4u);
        for (uint32_t face = 0; face < gts::rendering::EnvironmentCubemapFaceCount; ++face)
        {
            for (uint32_t y = 0; y < size; ++y)
            {
                for (uint32_t x = 0; x < size; ++x)
                {
                    const glm::vec3 direction = gts::rendering::cubemapDirectionForFaceUv(
                        static_cast<gts::rendering::EnvironmentCubemapFace>(face),
                        (static_cast<float>(x) + 0.5f) / static_cast<float>(size),
                        (static_cast<float>(y) + 0.5f) / static_cast<float>(size));
                    appendTexel(data, source.sampleDirection(direction));
                }
            }
        }
        return data;
    }

    inline std::vector<float> generateIrradianceCubemapData(const HdrEquirectangularImage& source,
                                                            uint32_t size)
    {
        std::vector<float> data;
        data.reserve(static_cast<size_t>(size) * size * gts::rendering::EnvironmentCubemapFaceCount * 4u);
        for (uint32_t face = 0; face < gts::rendering::EnvironmentCubemapFaceCount; ++face)
        {
            for (uint32_t y = 0; y < size; ++y)
            {
                for (uint32_t x = 0; x < size; ++x)
                {
                    const glm::vec3 normal = gts::rendering::cubemapDirectionForFaceUv(
                        static_cast<gts::rendering::EnvironmentCubemapFace>(face),
                        (static_cast<float>(x) + 0.5f) / static_cast<float>(size),
                        (static_cast<float>(y) + 0.5f) / static_cast<float>(size));
                    const glm::vec3 up =
                        std::abs(normal.y) < 0.999f ? glm::vec3(0.0f, 1.0f, 0.0f) : glm::vec3(0.0f, 0.0f, 1.0f);
                    const glm::vec3 right = gts::rendering::safeLightingNormalize(
                        glm::cross(up, normal),
                        {1.0f, 0.0f, 0.0f});
                    const glm::vec3 tangentUp = glm::cross(normal, right);

                    glm::vec3 irradiance{0.0f};
                    for (uint32_t phiIndex = 0; phiIndex < IrradiancePhiSamples; ++phiIndex)
                    {
                        const float phi = 2.0f * gts::rendering::PbrPi *
                            (static_cast<float>(phiIndex) + 0.5f) /
                            static_cast<float>(IrradiancePhiSamples);
                        for (uint32_t thetaIndex = 0; thetaIndex < IrradianceThetaSamples; ++thetaIndex)
                        {
                            const float theta = 0.5f * gts::rendering::PbrPi *
                                (static_cast<float>(thetaIndex) + 0.5f) /
                                static_cast<float>(IrradianceThetaSamples);
                            const float sinTheta = std::sin(theta);
                            const float cosTheta = std::cos(theta);
                            const glm::vec3 sampleDirection =
                                right * (std::cos(phi) * sinTheta) +
                                tangentUp * (std::sin(phi) * sinTheta) +
                                normal * cosTheta;
                            irradiance += source.sampleDirection(sampleDirection) * cosTheta * sinTheta;
                        }
                    }

                    const float sampleCount =
                        static_cast<float>(IrradiancePhiSamples * IrradianceThetaSamples);
                    appendTexel(data, irradiance * (gts::rendering::PbrPi / sampleCount));
                }
            }
        }
        return data;
    }

    inline std::vector<float> generatePrefilteredSpecularCubemapData(
        const HdrEquirectangularImage& source,
        uint32_t baseSize,
        uint32_t mipLevels)
    {
        std::vector<float> data;
        size_t texelCount = 0;
        for (uint32_t mip = 0; mip < mipLevels; ++mip)
        {
            const uint32_t size = std::max(1u, baseSize >> mip);
            texelCount += static_cast<size_t>(size) * size * gts::rendering::EnvironmentCubemapFaceCount;
        }
        data.reserve(texelCount * 4u);

        for (uint32_t mip = 0; mip < mipLevels; ++mip)
        {
            const uint32_t size = std::max(1u, baseSize >> mip);
            const float roughness = mipLevels > 1
                ? static_cast<float>(mip) / static_cast<float>(mipLevels - 1u)
                : 0.0f;
            for (uint32_t face = 0; face < gts::rendering::EnvironmentCubemapFaceCount; ++face)
            {
                for (uint32_t y = 0; y < size; ++y)
                {
                    for (uint32_t x = 0; x < size; ++x)
                    {
                        const glm::vec3 reflection = gts::rendering::cubemapDirectionForFaceUv(
                            static_cast<gts::rendering::EnvironmentCubemapFace>(face),
                            (static_cast<float>(x) + 0.5f) / static_cast<float>(size),
                            (static_cast<float>(y) + 0.5f) / static_cast<float>(size));
                        const glm::vec3 view = reflection;
                        glm::vec3 color{0.0f};
                        float totalWeight = 0.0f;
                        for (uint32_t i = 0; i < PrefilterSampleCount; ++i)
                        {
                            const glm::vec3 halfway = importanceSampleGgx(
                                hammersley2d(i, PrefilterSampleCount),
                                reflection,
                                roughness);
                            const glm::vec3 light = gts::rendering::safeLightingNormalize(
                                2.0f * glm::dot(view, halfway) * halfway - view,
                                reflection);
                            const float nDotL = gts::rendering::saturateLighting(glm::dot(reflection, light));
                            if (nDotL <= 0.0f)
                                continue;

                            color += source.sampleDirection(light) * nDotL;
                            totalWeight += nDotL;
                        }
                        appendTexel(data, totalWeight > 0.0f ? color / totalWeight : source.sampleDirection(reflection));
                    }
                }
            }
        }
        return data;
    }

    inline std::vector<float> generateBrdfLutData(uint32_t size)
    {
        std::vector<float> data;
        data.reserve(static_cast<size_t>(size) * size * 4u);
        for (uint32_t y = 0; y < size; ++y)
        {
            for (uint32_t x = 0; x < size; ++x)
            {
                const float nDotV = (static_cast<float>(x) + 0.5f) / static_cast<float>(size);
                const float roughness = (static_cast<float>(y) + 0.5f) / static_cast<float>(size);
                const glm::vec2 integrated = integrateBrdfReference(nDotV, roughness);
                data.push_back(std::max(0.0f, integrated.x));
                data.push_back(std::max(0.0f, integrated.y));
                data.push_back(0.0f);
                data.push_back(1.0f);
            }
        }
        return data;
    }

    inline std::vector<float> generateConstantCubemapData(const glm::vec3& color,
                                                          uint32_t baseSize,
                                                          uint32_t mipLevels)
    {
        std::vector<float> data;
        for (uint32_t mip = 0; mip < mipLevels; ++mip)
        {
            const uint32_t size = std::max(1u, baseSize >> mip);
            for (uint32_t i = 0; i < size * size * gts::rendering::EnvironmentCubemapFaceCount; ++i)
                appendTexel(data, color);
        }
        return data;
    }

    class VulkanEnvironmentImage
    {
    public:
        VulkanEnvironmentImage(VulkanBackendContext& backendContext,
                               uint32_t width,
                               uint32_t height,
                               uint32_t layers,
                               uint32_t mipLevels,
                               VkFormat format,
                               VkImageViewType viewType,
                               VkImageCreateFlags flags,
                               const std::vector<float>& rgbaData)
            : backendContext(backendContext)
            , width(width)
            , height(height)
            , layers(layers)
            , mipLevels(mipLevels)
            , format(format)
            , viewType(viewType)
        {
            createImage(flags);
            upload(rgbaData);
            createImageView();
            createSampler();
        }

        ~VulkanEnvironmentImage()
        {
            VkDevice device = backendContext.device();
            if (sampler != VK_NULL_HANDLE)
                vkDestroySampler(device, sampler, nullptr);
            if (imageView != VK_NULL_HANDLE)
                vkDestroyImageView(device, imageView, nullptr);
            if (image != VK_NULL_HANDLE)
                vkDestroyImage(device, image, nullptr);
            if (memory != VK_NULL_HANDLE)
                vkFreeMemory(device, memory, nullptr);
        }

        VulkanEnvironmentImage(const VulkanEnvironmentImage&) = delete;
        VulkanEnvironmentImage& operator=(const VulkanEnvironmentImage&) = delete;

        VkImageView getImageView() const { return imageView; }
        VkSampler getSampler() const { return sampler; }
        uint32_t getMipLevels() const { return mipLevels; }
        VkImageViewType getViewType() const { return viewType; }

    private:
        VulkanBackendContext& backendContext;
        uint32_t width = 0;
        uint32_t height = 0;
        uint32_t layers = 1;
        uint32_t mipLevels = 1;
        VkFormat format = VK_FORMAT_UNDEFINED;
        VkImageViewType viewType = VK_IMAGE_VIEW_TYPE_2D;
        VkImage image = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        VkImageView imageView = VK_NULL_HANDLE;
        VkSampler sampler = VK_NULL_HANDLE;

        void createImage(VkImageCreateFlags flags)
        {
            VkImageCreateInfo imageInfo{};
            imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
            imageInfo.flags = flags;
            imageInfo.imageType = VK_IMAGE_TYPE_2D;
            imageInfo.extent = {width, height, 1};
            imageInfo.mipLevels = mipLevels;
            imageInfo.arrayLayers = layers;
            imageInfo.format = format;
            imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
            imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
            imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
            imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

            if (vkCreateImage(backendContext.device(), &imageInfo, nullptr, &image) != VK_SUCCESS)
                throw std::runtime_error("failed to create environment image");

            MemoryUtil::allocateImageMemory(
                backendContext.device(),
                backendContext.physicalDevice(),
                image,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                memory);
        }

        void upload(const std::vector<float>& rgbaData)
        {
            VkDeviceSize imageSize = static_cast<VkDeviceSize>(rgbaData.size() * sizeof(float));
            VkBuffer stagingBuffer = VK_NULL_HANDLE;
            VkDeviceMemory stagingMemory = VK_NULL_HANDLE;
            BufferUtil::createBuffer(
                backendContext.device(),
                backendContext.physicalDevice(),
                imageSize,
                VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                stagingBuffer,
                stagingMemory);

            void* mapped = nullptr;
            vkMapMemory(backendContext.device(), stagingMemory, 0, imageSize, 0, &mapped);
            std::memcpy(mapped, rgbaData.data(), static_cast<size_t>(imageSize));
            vkUnmapMemory(backendContext.device(), stagingMemory);

            VkCommandBuffer commandBuffer =
                BufferUtil::beginSingleTimeCommands(backendContext.device(), backendContext.commandPool());
            transition(commandBuffer, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

            std::vector<VkBufferImageCopy> regions;
            regions.reserve(static_cast<size_t>(layers) * mipLevels);
            VkDeviceSize offset = 0;
            for (uint32_t mip = 0; mip < mipLevels; ++mip)
            {
                const uint32_t mipWidth = std::max(1u, width >> mip);
                const uint32_t mipHeight = std::max(1u, height >> mip);
                for (uint32_t layer = 0; layer < layers; ++layer)
                {
                    VkBufferImageCopy region{};
                    region.bufferOffset = offset;
                    region.bufferRowLength = 0;
                    region.bufferImageHeight = 0;
                    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                    region.imageSubresource.mipLevel = mip;
                    region.imageSubresource.baseArrayLayer = layer;
                    region.imageSubresource.layerCount = 1;
                    region.imageOffset = {0, 0, 0};
                    region.imageExtent = {mipWidth, mipHeight, 1};
                    regions.push_back(region);
                    offset += static_cast<VkDeviceSize>(mipWidth) *
                        static_cast<VkDeviceSize>(mipHeight) * 4u * sizeof(float);
                }
            }

            vkCmdCopyBufferToImage(
                commandBuffer,
                stagingBuffer,
                image,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                static_cast<uint32_t>(regions.size()),
                regions.data());

            transition(commandBuffer, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            BufferUtil::endSingleTimeCommands(
                backendContext.device(),
                backendContext.graphicsQueue(),
                backendContext.commandPool(),
                commandBuffer);

            vkDestroyBuffer(backendContext.device(), stagingBuffer, nullptr);
            vkFreeMemory(backendContext.device(), stagingMemory, nullptr);
        }

        void transition(VkCommandBuffer commandBuffer,
                        VkImageLayout oldLayout,
                        VkImageLayout newLayout)
        {
            VkImageMemoryBarrier barrier{};
            barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrier.oldLayout = oldLayout;
            barrier.newLayout = newLayout;
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.image = image;
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            barrier.subresourceRange.baseMipLevel = 0;
            barrier.subresourceRange.levelCount = mipLevels;
            barrier.subresourceRange.baseArrayLayer = 0;
            barrier.subresourceRange.layerCount = layers;

            VkPipelineStageFlags sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            VkPipelineStageFlags destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
            {
                barrier.srcAccessMask = 0;
                barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            }
            else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
                     newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
            {
                barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
                sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
                destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            }
            else
            {
                throw std::runtime_error("unsupported environment image transition");
            }

            vkCmdPipelineBarrier(
                commandBuffer,
                sourceStage,
                destinationStage,
                0,
                0,
                nullptr,
                0,
                nullptr,
                1,
                &barrier);
        }

        void createImageView()
        {
            VkImageViewCreateInfo viewInfo{};
            viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            viewInfo.image = image;
            viewInfo.viewType = viewType;
            viewInfo.format = format;
            viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            viewInfo.subresourceRange.baseMipLevel = 0;
            viewInfo.subresourceRange.levelCount = mipLevels;
            viewInfo.subresourceRange.baseArrayLayer = 0;
            viewInfo.subresourceRange.layerCount = layers;

            if (vkCreateImageView(backendContext.device(), &viewInfo, nullptr, &imageView) != VK_SUCCESS)
                throw std::runtime_error("failed to create environment image view");
        }

        void createSampler()
        {
            VkSamplerCreateInfo samplerInfo{};
            samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
            samplerInfo.magFilter = VK_FILTER_LINEAR;
            samplerInfo.minFilter = VK_FILTER_LINEAR;
            samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            samplerInfo.anisotropyEnable = VK_FALSE;
            samplerInfo.maxAnisotropy = 1.0f;
            samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
            samplerInfo.unnormalizedCoordinates = VK_FALSE;
            samplerInfo.compareEnable = VK_FALSE;
            samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
            samplerInfo.mipmapMode = mipLevels > 1
                ? VK_SAMPLER_MIPMAP_MODE_LINEAR
                : VK_SAMPLER_MIPMAP_MODE_NEAREST;
            samplerInfo.minLod = 0.0f;
            samplerInfo.maxLod = static_cast<float>(mipLevels - 1u);

            if (vkCreateSampler(backendContext.device(), &samplerInfo, nullptr, &sampler) != VK_SUCCESS)
                throw std::runtime_error("failed to create environment sampler");
        }
    };

    struct VulkanEnvironmentResources
    {
        std::unique_ptr<VulkanEnvironmentImage> radiance;
        std::unique_ptr<VulkanEnvironmentImage> irradiance;
        std::unique_ptr<VulkanEnvironmentImage> prefilteredSpecular;
        std::vector<VkDescriptorSet> descriptorSets;
        std::string cacheKey;
        uint32_t prefilteredMipCount = gts::rendering::EnvironmentPrefilterMipCount;
    };

    class EnvironmentResourceManager
    {
    public:
        EnvironmentResourceManager(VulkanBackendContext& backendContext,
                                   DescriptorSetManager& descriptorSetManager,
                                   uint32_t framesInFlight)
            : backendContext(backendContext)
            , descriptorSetManager(descriptorSetManager)
            , framesInFlight(framesInFlight)
        {
        }

        const std::vector<VkDescriptorSet>* getEnvironmentDescriptorSets(
            const gts::rendering::EnvironmentFrameData& environment)
        {
            VulkanEnvironmentResources* resources = nullptr;
            if (environment.enabled && !environment.environmentPath.empty())
            {
                resources = realizeEnvironment(environment.environmentPath);
            }

            if (resources == nullptr)
                resources = fallbackResources();

            return resources != nullptr ? &resources->descriptorSets : nullptr;
        }

    private:
        VulkanBackendContext& backendContext;
        DescriptorSetManager& descriptorSetManager;
        uint32_t framesInFlight = 0;
        std::unordered_map<std::string, std::unique_ptr<VulkanEnvironmentResources>> cache;
        std::unordered_set<std::string> failedCacheKeys;
        std::unique_ptr<VulkanEnvironmentResources> fallback;
        std::unique_ptr<VulkanEnvironmentImage> brdfLut;

        VulkanEnvironmentResources* realizeEnvironment(const std::string& authoredPath)
        {
            const std::string cacheKey = environmentCacheKeyForPath(authoredPath);
            auto existing = cache.find(cacheKey);
            if (existing != cache.end())
                return existing->second.get();
            if (failedCacheKeys.find(cacheKey) != failedCacheKeys.end())
                return nullptr;

            try
            {
                const std::string path = resolveEnvironmentAssetPath(authoredPath);
                HdrEquirectangularImage source = loadHdrEquirectangular(path);
                auto resources = createEnvironmentFromSource(source, cacheKey);
                auto [it, inserted] = cache.emplace(cacheKey, std::move(resources));
                return it->second.get();
            }
            catch (const std::exception& ex)
            {
                std::fprintf(stderr, "Environment realization failed for '%s': %s\n",
                             authoredPath.c_str(), ex.what());
                failedCacheKeys.insert(cacheKey);
                return nullptr;
            }
        }

        VulkanEnvironmentResources* fallbackResources()
        {
            if (fallback)
                return fallback.get();

            auto resources = std::make_unique<VulkanEnvironmentResources>();
            resources->cacheKey = "builtin:fallback-environment:"
                + std::to_string(EnvironmentPreprocessVersion);
            resources->radiance = createCubemap(
                RadianceCubemapSize,
                1,
                generateConstantCubemapData({0.0f, 0.0f, 0.0f}, RadianceCubemapSize, 1));
            resources->irradiance = createCubemap(
                IrradianceCubemapSize,
                1,
                generateConstantCubemapData({0.0f, 0.0f, 0.0f}, IrradianceCubemapSize, 1));
            resources->prefilteredSpecular = createCubemap(
                PrefilteredSpecularCubemapSize,
                gts::rendering::EnvironmentPrefilterMipCount,
                generateConstantCubemapData(
                    {0.0f, 0.0f, 0.0f},
                    PrefilteredSpecularCubemapSize,
                    gts::rendering::EnvironmentPrefilterMipCount));
            resources->descriptorSets = allocateDescriptorSets(*resources);
            fallback = std::move(resources);
            return fallback.get();
        }

        std::unique_ptr<VulkanEnvironmentResources> createEnvironmentFromSource(
            const HdrEquirectangularImage& source,
            const std::string& cacheKey)
        {
            auto resources = std::make_unique<VulkanEnvironmentResources>();
            resources->cacheKey = cacheKey;
            resources->radiance = createCubemap(
                RadianceCubemapSize,
                1,
                generateRadianceCubemapData(source, RadianceCubemapSize));
            resources->irradiance = createCubemap(
                IrradianceCubemapSize,
                1,
                generateIrradianceCubemapData(source, IrradianceCubemapSize));
            resources->prefilteredSpecular = createCubemap(
                PrefilteredSpecularCubemapSize,
                gts::rendering::EnvironmentPrefilterMipCount,
                generatePrefilteredSpecularCubemapData(
                    source,
                    PrefilteredSpecularCubemapSize,
                    gts::rendering::EnvironmentPrefilterMipCount));
            resources->descriptorSets = allocateDescriptorSets(*resources);
            return resources;
        }

        std::unique_ptr<VulkanEnvironmentImage> createCubemap(uint32_t size,
                                                              uint32_t mipLevels,
                                                              const std::vector<float>& data)
        {
            return std::make_unique<VulkanEnvironmentImage>(
                backendContext,
                size,
                size,
                gts::rendering::EnvironmentCubemapFaceCount,
                mipLevels,
                EnvironmentHdrFormat,
                VK_IMAGE_VIEW_TYPE_CUBE,
                VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT,
                data);
        }

        VulkanEnvironmentImage& sharedBrdfLut()
        {
            if (!brdfLut)
            {
                brdfLut = std::make_unique<VulkanEnvironmentImage>(
                    backendContext,
                    BrdfLutSize,
                    BrdfLutSize,
                    1,
                    1,
                    EnvironmentBrdfFormat,
                    VK_IMAGE_VIEW_TYPE_2D,
                    0,
                    generateBrdfLutData(BrdfLutSize));
            }
            return *brdfLut;
        }

        std::vector<VkDescriptorSet> allocateDescriptorSets(VulkanEnvironmentResources& resources)
        {
            std::array<VkDescriptorImageInfo, DescriptorSetManager::EnvironmentTextureBindingCount> images{};
            images[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            images[0].imageView = resources.irradiance->getImageView();
            images[0].sampler = resources.irradiance->getSampler();
            images[1].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            images[1].imageView = resources.prefilteredSpecular->getImageView();
            images[1].sampler = resources.prefilteredSpecular->getSampler();
            images[2].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            images[2].imageView = sharedBrdfLut().getImageView();
            images[2].sampler = sharedBrdfLut().getSampler();
            return descriptorSetManager.allocateForEnvironmentTextures(images);
        }
    };
}
