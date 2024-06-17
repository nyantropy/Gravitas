#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm.hpp>
#include <gtc/matrix_transform.hpp>
#include <gtx/hash.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

#include <iostream>
#include <fstream>
#include <stdexcept>
#include <algorithm>
#include <chrono>
#include <vector>
#include <cstring>
#include <cstdlib>
#include <cstdint>
#include <limits>
#include <array>
#include <optional>
#include <set>
#include <unordered_map>

#include "GravitasEngineConstants.h"
#include "Vertex.h"
#include "UniformBufferObject.h"

#include "GTSGLFWOutputWindow.hpp"
#include "GLFWWindowSurface.hpp"
#include "VulkanPhysicalDevice.hpp"
#include "VulkanLogicalDevice.hpp"
#include "VulkanSwapChain.hpp"
#include "VulkanRenderer.hpp"
#include "VulkanRenderPass.hpp"
#include "GTSDescriptorSetManager.hpp"
#include "VulkanShader.hpp"
#include "VulkanPipeline.hpp"

const std::string MODEL_PATH = "resources/viking_room.obj";
const std::string TEXTURE_PATH = "resources/viking_room.png";

#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif

// VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger) {
//     auto func = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
//     if (func != nullptr) {
//         return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
//     } else {
//         return VK_ERROR_EXTENSION_NOT_PRESENT;
//     }
// }

// void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator) {
//     auto func = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
//     if (func != nullptr) {
//         func(instance, debugMessenger, pAllocator);
//     }
// }

namespace std 
{
    template<> struct hash<Vertex> 
    {
        size_t operator()(Vertex const& vertex) const 
        {
            return (std::hash<glm::vec3>()(vertex.pos) ^ (std::hash<glm::vec3>()(vertex.color) << 1)) >> 1 ^ (std::hash<glm::vec2>()(vertex.texCoord) << 1);
        }
    };
}

class Gravitas 
{
public:
    GTSOutputWindow* vwindow;
    VulkanInstance* vinstance;
    WindowSurface* vsurface;
    VulkanPhysicalDevice* vphysicaldevice;
    VulkanLogicalDevice* vlogicaldevice;
    VulkanSwapChain* vswapchain;
    VulkanRenderer* vrenderer;
    VulkanRenderPass* vrenderpass;
    GTSDescriptorSetManager* vdescriptorsetmanager;
    VulkanPipeline* vpipeline;

    void run() 
    {
        vwindow = new GTSGLFWOutputWindow();
        vwindow->init(GravitasEngineConstants::GLFW_DEFAULT_WIDTH, GravitasEngineConstants::GLFW_DEFAULT_HEIGHT, "Title");
        vwindow->setOnWindowResizeCallback(std::bind(&Gravitas::OnFrameBufferResizeCallback, this, std::placeholders::_1, std::placeholders::_2));

        vinstance = new VulkanInstance(enableValidationLayers);
        vsurface = new GLFWWindowSurface(vwindow, vinstance);
        vphysicaldevice = new VulkanPhysicalDevice(vinstance, vsurface);
        vlogicaldevice = new VulkanLogicalDevice(vinstance, vphysicaldevice, enableValidationLayers);
        vswapchain = new VulkanSwapChain(vwindow, vsurface, vphysicaldevice, vlogicaldevice);
        vrenderer = new VulkanRenderer(vlogicaldevice, vphysicaldevice, vswapchain);

        vrenderpass = new VulkanRenderPass();
        vrenderpass->init(vswapchain, vlogicaldevice, vrenderer);

        vdescriptorsetmanager = new GTSDescriptorSetManager(vlogicaldevice, GravitasEngineConstants::MAX_FRAMES_IN_FLIGHT);
        vpipeline = new VulkanPipeline(vlogicaldevice, vdescriptorsetmanager, vrenderpass, {GravitasEngineConstants::V_SHADER_PATH, GravitasEngineConstants::F_SHADER_PATH});





        initVulkan();
        mainLoop();
        cleanup();

        delete vpipeline;
        delete vdescriptorsetmanager;
        delete vrenderpass;
        delete vrenderer;
        delete vswapchain;
        delete vlogicaldevice;
        delete vphysicaldevice;
        delete vsurface;
        delete vinstance;
        delete vwindow;
    }

private:

    //VkDebugUtilsMessengerEXT debugMessenger;

    VkImage textureImage;
    VkDeviceMemory textureImageMemory;
    VkImageView textureImageView;
    VkSampler textureSampler;

    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    VkBuffer vertexBuffer;
    VkDeviceMemory vertexBufferMemory;
    VkBuffer indexBuffer;
    VkDeviceMemory indexBufferMemory;

    std::vector<VkBuffer> uniformBuffers;
    std::vector<VkDeviceMemory> uniformBuffersMemory;
    std::vector<void*> uniformBuffersMapped;

    std::vector<VkDescriptorSet> descriptorSets;

    std::vector<VkCommandBuffer> commandBuffers;

    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> renderFinishedSemaphores;
    std::vector<VkFence> inFlightFences;
    uint32_t currentFrame = 0;

    bool framebufferResized = false;

    void OnFrameBufferResizeCallback(int width, int height) 
    {
        framebufferResized = true;
    }

    void initVulkan() 
    {
        createFramebuffers();
        createTextureImage();
        createTextureImageView();
        createTextureSampler();
        loadModel();
        createVertexBuffer();
        createIndexBuffer();
        createUniformBuffers();
        createDescriptorSets();
        createCommandBuffers();
        createSyncObjects();
    }

    void mainLoop() 
    {
        while (!vwindow->shouldClose())
        {
            vwindow->pollEvents();
            drawFrame();
        }

        vkDeviceWaitIdle(vlogicaldevice->getDevice());
    }

    void cleanupSwapChain() 
    {
        //vkDestroyImageView(vlogicaldevice->getDevice(), vrenderer->getDepthImageView(), nullptr);
        //vkDestroyImage(vlogicaldevice->getDevice(), vrenderer->getDepthImage(), nullptr);
        //vkFreeMemory(vlogicaldevice->getDevice(), vrenderer->getDepthImageMemory(), nullptr);

        for (auto framebuffer : vswapchain->getSwapChainFramebuffers()) {
            vkDestroyFramebuffer(vlogicaldevice->getDevice(), framebuffer, nullptr);
        }

        for (auto imageView : vswapchain->getSwapChainImageViews()) {
            vkDestroyImageView(vlogicaldevice->getDevice(), imageView, nullptr);
        }
    }

    void cleanup() {
        cleanupSwapChain();

        for (size_t i = 0; i < GravitasEngineConstants::MAX_FRAMES_IN_FLIGHT; i++) {
            vkDestroyBuffer(vlogicaldevice->getDevice(), uniformBuffers[i], nullptr);
            vkFreeMemory(vlogicaldevice->getDevice(), uniformBuffersMemory[i], nullptr);
        }

        

        vkDestroySampler(vlogicaldevice->getDevice(), textureSampler, nullptr);
        vkDestroyImageView(vlogicaldevice->getDevice(), textureImageView, nullptr);

        vkDestroyImage(vlogicaldevice->getDevice(), textureImage, nullptr);
        vkFreeMemory(vlogicaldevice->getDevice(), textureImageMemory, nullptr);

        vkDestroyBuffer(vlogicaldevice->getDevice(), indexBuffer, nullptr);
        vkFreeMemory(vlogicaldevice->getDevice(), indexBufferMemory, nullptr);

        vkDestroyBuffer(vlogicaldevice->getDevice(), vertexBuffer, nullptr);
        vkFreeMemory(vlogicaldevice->getDevice(), vertexBufferMemory, nullptr);

        for (size_t i = 0; i < GravitasEngineConstants::MAX_FRAMES_IN_FLIGHT; i++) {
            vkDestroySemaphore(vlogicaldevice->getDevice(), renderFinishedSemaphores[i], nullptr);
            vkDestroySemaphore(vlogicaldevice->getDevice(), imageAvailableSemaphores[i], nullptr);
            vkDestroyFence(vlogicaldevice->getDevice(), inFlightFences[i], nullptr);
        }
    }

    void recreateSwapChain() 
    {
        int width = 0, height = 0;
        vwindow->getSize(width, height);

        while (width == 0 || height == 0) 
        {
            vwindow->getSize(width, height);
            vwindow->pollEvents();
        }

        std::cout << "swapchain recreated" << std::endl;
        std::cout << width << std::endl;
        std::cout << height << std::endl;

        vkDeviceWaitIdle(vlogicaldevice->getDevice());

        cleanupSwapChain();

        delete vswapchain;
        vswapchain = new VulkanSwapChain(vwindow, vsurface, vphysicaldevice, vlogicaldevice);
        //createDepthResources();
        createFramebuffers();
    }

    // void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo) 
    // {
    //     createInfo = {};
    //     createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    //     createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    //     createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    //     createInfo.pfnUserCallback = debugCallback;
    // }

    void createFramebuffers() 
    {
        vswapchain->getSwapChainFramebuffers().resize(vswapchain->getSwapChainImageViews().size());

        for (size_t i = 0; i < vswapchain->getSwapChainImageViews().size(); i++) {
            std::array<VkImageView, 2> attachments = {
                vswapchain->getSwapChainImageViews()[i],
                vrenderer->getDepthImageView()
            };

            VkFramebufferCreateInfo framebufferInfo{};
            framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferInfo.renderPass = vrenderpass->getRenderPass();
            framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
            framebufferInfo.pAttachments = attachments.data();
            framebufferInfo.width = vswapchain->getSwapChainExtent().width;
            framebufferInfo.height = vswapchain->getSwapChainExtent().height;
            framebufferInfo.layers = 1;

            if (vkCreateFramebuffer(vlogicaldevice->getDevice(), &framebufferInfo, nullptr, &vswapchain->getSwapChainFramebuffers()[i]) != VK_SUCCESS) {
                throw std::runtime_error("failed to create framebuffer!");
            }
        }
    }

    bool hasStencilComponent(VkFormat format) {
        return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
    }

    void createTextureImage() {
        int texWidth, texHeight, texChannels;
        stbi_uc* pixels = stbi_load(TEXTURE_PATH.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
        VkDeviceSize imageSize = texWidth * texHeight * 4;

        if (!pixels) {
            throw std::runtime_error("failed to load texture image!");
        }

        VkBuffer stagingBuffer;
        VkDeviceMemory stagingBufferMemory;
        createBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

        void* data;
        vkMapMemory(vlogicaldevice->getDevice(), stagingBufferMemory, 0, imageSize, 0, &data);
            memcpy(data, pixels, static_cast<size_t>(imageSize));
        vkUnmapMemory(vlogicaldevice->getDevice(), stagingBufferMemory);

        stbi_image_free(pixels);

        vrenderer->createImage(texWidth, texHeight, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, textureImage, textureImageMemory);

        transitionImageLayout(textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
            copyBufferToImage(stagingBuffer, textureImage, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));
        transitionImageLayout(textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        vkDestroyBuffer(vlogicaldevice->getDevice(), stagingBuffer, nullptr);
        vkFreeMemory(vlogicaldevice->getDevice(), stagingBufferMemory, nullptr);
    }

    void createTextureImageView() {
        textureImageView = vswapchain->createImageView(textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT);
    }

    void createTextureSampler() {
        VkPhysicalDeviceProperties properties{};
        vkGetPhysicalDeviceProperties(vphysicaldevice->getPhysicalDevice(), &properties);

        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.anisotropyEnable = VK_TRUE;
        samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
        samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        samplerInfo.unnormalizedCoordinates = VK_FALSE;
        samplerInfo.compareEnable = VK_FALSE;
        samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

        if (vkCreateSampler(vlogicaldevice->getDevice(), &samplerInfo, nullptr, &textureSampler) != VK_SUCCESS) 
        {
            throw std::runtime_error("failed to create texture sampler!");
        }
    }

    void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout) {
        VkCommandBuffer commandBuffer = beginSingleTimeCommands();

        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = oldLayout;
        barrier.newLayout = newLayout;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;

        VkPipelineStageFlags sourceStage;
        VkPipelineStageFlags destinationStage;

        if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

            sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

            sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        } else {
            throw std::invalid_argument("unsupported layout transition!");
        }

        vkCmdPipelineBarrier(
            commandBuffer,
            sourceStage, destinationStage,
            0,
            0, nullptr,
            0, nullptr,
            1, &barrier
        );

        endSingleTimeCommands(commandBuffer);
    }

    void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height) {
        VkCommandBuffer commandBuffer = beginSingleTimeCommands();

        VkBufferImageCopy region{};
        region.bufferOffset = 0;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;
        region.imageOffset = {0, 0, 0};
        region.imageExtent = {
            width,
            height,
            1
        };

        vkCmdCopyBufferToImage(commandBuffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

        endSingleTimeCommands(commandBuffer);
    }

    void loadModel() {
        tinyobj::attrib_t attrib;
        std::vector<tinyobj::shape_t> shapes;
        std::vector<tinyobj::material_t> materials;
        std::string warn, err;

        if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, MODEL_PATH.c_str())) {
            throw std::runtime_error(warn + err);
        }

        std::unordered_map<Vertex, uint32_t> uniqueVertices{};

        for (const auto& shape : shapes) {
            for (const auto& index : shape.mesh.indices) {
                Vertex vertex{};

                vertex.pos = {
                    attrib.vertices[3 * index.vertex_index + 0],
                    attrib.vertices[3 * index.vertex_index + 1],
                    attrib.vertices[3 * index.vertex_index + 2]
                };

                vertex.texCoord = {
                    attrib.texcoords[2 * index.texcoord_index + 0],
                    1.0f - attrib.texcoords[2 * index.texcoord_index + 1]
                };

                vertex.color = {1.0f, 1.0f, 1.0f};

                if (uniqueVertices.count(vertex) == 0) {
                    uniqueVertices[vertex] = static_cast<uint32_t>(vertices.size());
                    vertices.push_back(vertex);
                }

                indices.push_back(uniqueVertices[vertex]);
            }
        }
    }

    void createVertexBuffer() {
        VkDeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();

        VkBuffer stagingBuffer;
        VkDeviceMemory stagingBufferMemory;
        createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

        void* data;
        vkMapMemory(vlogicaldevice->getDevice(), stagingBufferMemory, 0, bufferSize, 0, &data);
            memcpy(data, vertices.data(), (size_t) bufferSize);
        vkUnmapMemory(vlogicaldevice->getDevice(), stagingBufferMemory);

        createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vertexBuffer, vertexBufferMemory);

        copyBuffer(stagingBuffer, vertexBuffer, bufferSize);

        vkDestroyBuffer(vlogicaldevice->getDevice(), stagingBuffer, nullptr);
        vkFreeMemory(vlogicaldevice->getDevice(), stagingBufferMemory, nullptr);
    }

    void createIndexBuffer() {
        VkDeviceSize bufferSize = sizeof(indices[0]) * indices.size();

        VkBuffer stagingBuffer;
        VkDeviceMemory stagingBufferMemory;
        createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

        void* data;
        vkMapMemory(vlogicaldevice->getDevice(), stagingBufferMemory, 0, bufferSize, 0, &data);
            memcpy(data, indices.data(), (size_t) bufferSize);
        vkUnmapMemory(vlogicaldevice->getDevice(), stagingBufferMemory);

        createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, indexBuffer, indexBufferMemory);

        copyBuffer(stagingBuffer, indexBuffer, bufferSize);

        vkDestroyBuffer(vlogicaldevice->getDevice(), stagingBuffer, nullptr);
        vkFreeMemory(vlogicaldevice->getDevice(), stagingBufferMemory, nullptr);
    }

    void createUniformBuffers() {
        VkDeviceSize bufferSize = sizeof(UniformBufferObject);

        uniformBuffers.resize(GravitasEngineConstants::MAX_FRAMES_IN_FLIGHT);
        uniformBuffersMemory.resize(GravitasEngineConstants::MAX_FRAMES_IN_FLIGHT);
        uniformBuffersMapped.resize(GravitasEngineConstants::MAX_FRAMES_IN_FLIGHT);

        for (size_t i = 0; i < GravitasEngineConstants::MAX_FRAMES_IN_FLIGHT; i++) {
            createBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, uniformBuffers[i], uniformBuffersMemory[i]);

            vkMapMemory(vlogicaldevice->getDevice(), uniformBuffersMemory[i], 0, bufferSize, 0, &uniformBuffersMapped[i]);
        }
    }

    void createDescriptorSets() {
        std::vector<VkDescriptorSetLayout> layouts(GravitasEngineConstants::MAX_FRAMES_IN_FLIGHT, vdescriptorsetmanager->getDescriptorSetLayout());
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = vdescriptorsetmanager->getDescriptorPool();
        allocInfo.descriptorSetCount = static_cast<uint32_t>(GravitasEngineConstants::MAX_FRAMES_IN_FLIGHT);
        allocInfo.pSetLayouts = layouts.data();

        descriptorSets.resize(GravitasEngineConstants::MAX_FRAMES_IN_FLIGHT);
        if (vkAllocateDescriptorSets(vlogicaldevice->getDevice(), &allocInfo, descriptorSets.data()) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate descriptor sets!");
        }

        for (size_t i = 0; i < GravitasEngineConstants::MAX_FRAMES_IN_FLIGHT; i++) {
            VkDescriptorBufferInfo bufferInfo{};
            bufferInfo.buffer = uniformBuffers[i];
            bufferInfo.offset = 0;
            bufferInfo.range = sizeof(UniformBufferObject);

            VkDescriptorImageInfo imageInfo{};
            imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            imageInfo.imageView = textureImageView;
            imageInfo.sampler = textureSampler;

            std::array<VkWriteDescriptorSet, 2> descriptorWrites{};

            descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[0].dstSet = descriptorSets[i];
            descriptorWrites[0].dstBinding = 0;
            descriptorWrites[0].dstArrayElement = 0;
            descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            descriptorWrites[0].descriptorCount = 1;
            descriptorWrites[0].pBufferInfo = &bufferInfo;

            descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[1].dstSet = descriptorSets[i];
            descriptorWrites[1].dstBinding = 1;
            descriptorWrites[1].dstArrayElement = 0;
            descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descriptorWrites[1].descriptorCount = 1;
            descriptorWrites[1].pImageInfo = &imageInfo;

            vkUpdateDescriptorSets(vlogicaldevice->getDevice(), static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
        }
    }

    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory) {
        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = size;
        bufferInfo.usage = usage;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if (vkCreateBuffer(vlogicaldevice->getDevice(), &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
            throw std::runtime_error("failed to create buffer!");
        }

        VkMemoryRequirements memRequirements;
        vkGetBufferMemoryRequirements(vlogicaldevice->getDevice(), buffer, &memRequirements);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = vphysicaldevice->findMemoryType(memRequirements.memoryTypeBits, properties);

        if (vkAllocateMemory(vlogicaldevice->getDevice(), &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate buffer memory!");
        }

        vkBindBufferMemory(vlogicaldevice->getDevice(), buffer, bufferMemory, 0);
    }

    VkCommandBuffer beginSingleTimeCommands() 
    {
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandPool = vlogicaldevice->getCommandPool();
        allocInfo.commandBufferCount = 1;

        VkCommandBuffer commandBuffer;
        vkAllocateCommandBuffers(vlogicaldevice->getDevice(), &allocInfo, &commandBuffer);

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        vkBeginCommandBuffer(commandBuffer, &beginInfo);

        return commandBuffer;
    }

    void endSingleTimeCommands(VkCommandBuffer commandBuffer) {
        vkEndCommandBuffer(commandBuffer);

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;

        vkQueueSubmit(vlogicaldevice->getGraphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(vlogicaldevice->getGraphicsQueue());

        vkFreeCommandBuffers(vlogicaldevice->getDevice(), vlogicaldevice->getCommandPool(), 1, &commandBuffer);
    }

    void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {
        VkCommandBuffer commandBuffer = beginSingleTimeCommands();

        VkBufferCopy copyRegion{};
        copyRegion.size = size;
        vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

        endSingleTimeCommands(commandBuffer);
    }

    void createCommandBuffers() {
        commandBuffers.resize(GravitasEngineConstants::MAX_FRAMES_IN_FLIGHT);

        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = vlogicaldevice->getCommandPool();
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = (uint32_t) commandBuffers.size();

        if (vkAllocateCommandBuffers(vlogicaldevice->getDevice(), &allocInfo, commandBuffers.data()) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate command buffers!");
        }
    }

    void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex) {
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

        if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
            throw std::runtime_error("failed to begin recording command buffer!");
        }

        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = vrenderpass->getRenderPass();
        renderPassInfo.framebuffer = vswapchain->getSwapChainFramebuffers()[imageIndex];
        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent = vswapchain->getSwapChainExtent();

        std::array<VkClearValue, 2> clearValues{};
        clearValues[0].color = {{0.0f, 0.0f, 0.0f, 1.0f}};
        clearValues[1].depthStencil = {1.0f, 0};

        renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
        renderPassInfo.pClearValues = clearValues.data();

        vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vpipeline->getPipeline());

            VkViewport viewport{};
            viewport.x = 0.0f;
            viewport.y = 0.0f;
            viewport.width = (float) vswapchain->getSwapChainExtent().width;
            viewport.height = (float) vswapchain->getSwapChainExtent().height;
            viewport.minDepth = 0.0f;
            viewport.maxDepth = 1.0f;
            vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

            VkRect2D scissor{};
            scissor.offset = {0, 0};
            scissor.extent = vswapchain->getSwapChainExtent();
            vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

            VkBuffer vertexBuffers[] = {vertexBuffer};
            VkDeviceSize offsets[] = {0};
            vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

            vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);

            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vpipeline->getPipelineLayout(), 0, 1, &descriptorSets[currentFrame], 0, nullptr);

            vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);

        vkCmdEndRenderPass(commandBuffer);

        if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
            throw std::runtime_error("failed to record command buffer!");
        }
    }

    void createSyncObjects() {
        imageAvailableSemaphores.resize(GravitasEngineConstants::MAX_FRAMES_IN_FLIGHT);
        renderFinishedSemaphores.resize(GravitasEngineConstants::MAX_FRAMES_IN_FLIGHT);
        inFlightFences.resize(GravitasEngineConstants::MAX_FRAMES_IN_FLIGHT);

        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        for (size_t i = 0; i < GravitasEngineConstants::MAX_FRAMES_IN_FLIGHT; i++) {
            if (vkCreateSemaphore(vlogicaldevice->getDevice(), &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS ||
                vkCreateSemaphore(vlogicaldevice->getDevice(), &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS ||
                vkCreateFence(vlogicaldevice->getDevice(), &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS) {
                throw std::runtime_error("failed to create synchronization objects for a frame!");
            }
        }
    }

    void updateUniformBuffer(uint32_t currentImage) {
        static auto startTime = std::chrono::high_resolution_clock::now();

        auto currentTime = std::chrono::high_resolution_clock::now();
        float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

        UniformBufferObject ubo{};
        ubo.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
        ubo.view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
        ubo.proj = glm::perspective(glm::radians(45.0f), vswapchain->getSwapChainExtent().width / (float) vswapchain->getSwapChainExtent().height, 0.1f, 10.0f);
        ubo.proj[1][1] *= -1;

        memcpy(uniformBuffersMapped[currentImage], &ubo, sizeof(ubo));
    }

    void drawFrame() {
        vkWaitForFences(vlogicaldevice->getDevice(), 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);

        uint32_t imageIndex;
        VkResult result = vkAcquireNextImageKHR(vlogicaldevice->getDevice(), vswapchain->getSwapChain(), UINT64_MAX, imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);

        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            recreateSwapChain();
            return;
        } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
            throw std::runtime_error("failed to acquire swap chain image!");
        }

        updateUniformBuffer(currentFrame);

        vkResetFences(vlogicaldevice->getDevice(), 1, &inFlightFences[currentFrame]);

        vkResetCommandBuffer(commandBuffers[currentFrame], /*VkCommandBufferResetFlagBits*/ 0);
        recordCommandBuffer(commandBuffers[currentFrame], imageIndex);

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

        VkSemaphore waitSemaphores[] = {imageAvailableSemaphores[currentFrame]};
        VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphores;
        submitInfo.pWaitDstStageMask = waitStages;

        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffers[currentFrame];

        VkSemaphore signalSemaphores[] = {renderFinishedSemaphores[currentFrame]};
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signalSemaphores;

        if (vkQueueSubmit(vlogicaldevice->getGraphicsQueue(), 1, &submitInfo, inFlightFences[currentFrame]) != VK_SUCCESS) {
            throw std::runtime_error("failed to submit draw command buffer!");
        }

        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = signalSemaphores;

        VkSwapchainKHR swapChains[] = {vswapchain->getSwapChain()};
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = swapChains;

        presentInfo.pImageIndices = &imageIndex;

        result = vkQueuePresentKHR(vlogicaldevice->getPresentQueue(), &presentInfo);

        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || framebufferResized) {
            framebufferResized = false;
            std::cout << "FrameBuffer was resized! recreating swapchain" << std::endl;
            recreateSwapChain();
        } else if (result != VK_SUCCESS) {
            throw std::runtime_error("failed to present swap chain image!");
        }

        currentFrame = (currentFrame + 1) % GravitasEngineConstants::MAX_FRAMES_IN_FLIGHT;
    }

    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData) 
    {
        std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;

        return VK_FALSE;
    }
};