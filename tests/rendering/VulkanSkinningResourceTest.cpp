#include <array>
#include <cstdio>
#include <cstring>
#include <limits>
#include <map>
#include <stdexcept>
#include <vector>
#include "VulkanSkinPaletteBuffer.h"
#include "VulkanSkinnedMeshResource.h"
#include "VulkanSkinnedPipelineConfig.h"
#include "animation/skinning/GtsSkinPalette.h"

// Test-only Vulkan entry-point seam: inspect uploaded bytes and recorded bindings without a device.
namespace
{
    uintptr_t            nextHandle = 10;
    template <class T> T handle()
    {
        return reinterpret_cast<T>(nextHandle++);
    }
    struct Buffer
    {
        VkDeviceSize       size;
        VkBufferUsageFlags usage;
        VkDeviceMemory     memory{};
    };
    std::map<VkBuffer, Buffer>                           buffers;
    std::map<VkDeviceMemory, std::vector<unsigned char>> allocations;
    std::map<VkDescriptorSet, VkDescriptorBufferInfo>    descriptors;
    size_t                                               createdBuffers = 0;
    bool                                                 failMap        = false;
    VkDescriptorSet                                      boundPalette{};
    std::array<VkBuffer, 2>                              boundVertices{};
    VkBuffer                                             boundIndices{};
    uint32_t                                             drawCount = 0, drawFirst = 0;
    void                                                 require(bool value, const char* message)
    {
        if (!value)
            throw std::runtime_error(message);
    }
    template <class F> void rejects(F action)
    {
        try
        {
            action();
        }
        catch (const std::exception&)
        {
            return;
        }
        throw std::runtime_error("Invalid operation accepted");
    }
    std::vector<unsigned char> bytes(VkDescriptorSet set)
    {
        const auto  info = descriptors.at(set);
        const auto& data = allocations.at(buffers.at(info.buffer).memory);
        return {data.begin(), data.begin() + info.range};
    }
} // namespace

extern "C"
{
    VKAPI_ATTR void VKAPI_CALL vkGetPhysicalDeviceProperties(VkPhysicalDevice, VkPhysicalDeviceProperties* properties)
    {
        *properties                                            = {};
        properties->limits.maxStorageBufferRange               = 65536;
        properties->limits.maxBoundDescriptorSets              = 8;
        properties->limits.maxPerStageDescriptorStorageBuffers = 8;
    }
    VKAPI_ATTR void VKAPI_CALL vkGetPhysicalDeviceMemoryProperties(VkPhysicalDevice,
                                                                   VkPhysicalDeviceMemoryProperties* properties)
    {
        *properties                 = {};
        properties->memoryTypeCount = 1;
        properties->memoryTypes[0].propertyFlags =
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    }
    VKAPI_ATTR VkResult VKAPI_CALL vkCreateBuffer(VkDevice,
                                                  const VkBufferCreateInfo* info,
                                                  const VkAllocationCallbacks*,
                                                  VkBuffer* result)
    {
        *result          = handle<VkBuffer>();
        buffers[*result] = {info->size, info->usage};
        ++createdBuffers;
        return VK_SUCCESS;
    }
    VKAPI_ATTR void VKAPI_CALL vkGetBufferMemoryRequirements(VkDevice,
                                                             VkBuffer              buffer,
                                                             VkMemoryRequirements* requirements)
    {
        *requirements = {buffers.at(buffer).size, 16, 1};
    }
    VKAPI_ATTR VkResult VKAPI_CALL vkAllocateMemory(VkDevice,
                                                    const VkMemoryAllocateInfo* info,
                                                    const VkAllocationCallbacks*,
                                                    VkDeviceMemory* result)
    {
        *result = handle<VkDeviceMemory>();
        allocations[*result].resize(info->allocationSize);
        return VK_SUCCESS;
    }
    VKAPI_ATTR VkResult VKAPI_CALL vkBindBufferMemory(VkDevice, VkBuffer buffer, VkDeviceMemory memory, VkDeviceSize)
    {
        buffers.at(buffer).memory = memory;
        return VK_SUCCESS;
    }
    VKAPI_ATTR VkResult VKAPI_CALL
    vkMapMemory(VkDevice, VkDeviceMemory memory, VkDeviceSize, VkDeviceSize, VkMemoryMapFlags, void** mapped)
    {
        if (failMap)
            return VK_ERROR_MEMORY_MAP_FAILED;
        *mapped = allocations.at(memory).data();
        return VK_SUCCESS;
    }
    VKAPI_ATTR void VKAPI_CALL vkUnmapMemory(VkDevice, VkDeviceMemory) {}
    VKAPI_ATTR void VKAPI_CALL vkDestroyBuffer(VkDevice, VkBuffer buffer, const VkAllocationCallbacks*)
    {
        buffers.erase(buffer);
    }
    VKAPI_ATTR void VKAPI_CALL vkFreeMemory(VkDevice, VkDeviceMemory memory, const VkAllocationCallbacks*)
    {
        allocations.erase(memory);
    }
    VKAPI_ATTR VkResult VKAPI_CALL vkCreateDescriptorSetLayout(VkDevice,
                                                               const VkDescriptorSetLayoutCreateInfo* info,
                                                               const VkAllocationCallbacks*,
                                                               VkDescriptorSetLayout* result)
    {
        require(info->bindingCount == 1 && info->pBindings[0].binding == 0 &&
                    info->pBindings[0].descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                "Palette layout");
        *result = handle<VkDescriptorSetLayout>();
        return VK_SUCCESS;
    }
    VKAPI_ATTR void VKAPI_CALL vkDestroyDescriptorSetLayout(VkDevice,
                                                            VkDescriptorSetLayout,
                                                            const VkAllocationCallbacks*)
    {
    }
    VKAPI_ATTR VkResult VKAPI_CALL vkCreateDescriptorPool(VkDevice,
                                                          const VkDescriptorPoolCreateInfo*,
                                                          const VkAllocationCallbacks*,
                                                          VkDescriptorPool* result)
    {
        *result = handle<VkDescriptorPool>();
        return VK_SUCCESS;
    }
    VKAPI_ATTR void VKAPI_CALL     vkDestroyDescriptorPool(VkDevice, VkDescriptorPool, const VkAllocationCallbacks*) {}
    VKAPI_ATTR VkResult VKAPI_CALL vkAllocateDescriptorSets(VkDevice,
                                                            const VkDescriptorSetAllocateInfo* info,
                                                            VkDescriptorSet*                   result)
    {
        for (uint32_t i = 0; i < info->descriptorSetCount; ++i)
            result[i] = handle<VkDescriptorSet>();
        return VK_SUCCESS;
    }
    VKAPI_ATTR void VKAPI_CALL vkUpdateDescriptorSets(
        VkDevice, uint32_t count, const VkWriteDescriptorSet* writes, uint32_t, const VkCopyDescriptorSet*)
    {
        require(count == 1 && writes->dstBinding == 0 && writes->descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                "Palette storage descriptor write");
        descriptors[writes->dstSet] = *writes->pBufferInfo;
    }
    VKAPI_ATTR void VKAPI_CALL vkCmdBindDescriptorSets(VkCommandBuffer,
                                                       VkPipelineBindPoint,
                                                       VkPipelineLayout,
                                                       uint32_t               first,
                                                       uint32_t               count,
                                                       const VkDescriptorSet* sets,
                                                       uint32_t,
                                                       const uint32_t*)
    {
        require(first == 4 && count == 1, "Draw palette set");
        boundPalette = *sets;
    }
    VKAPI_ATTR void VKAPI_CALL vkCmdBindVertexBuffers(
        VkCommandBuffer, uint32_t first, uint32_t count, const VkBuffer* data, const VkDeviceSize* offsets)
    {
        require(first == 0 && count == 2 && offsets[0] == 0 && offsets[1] == 0, "Vertex and object instance bindings");
        boundVertices = {data[0], data[1]};
    }
    VKAPI_ATTR void VKAPI_CALL vkCmdBindIndexBuffer(VkCommandBuffer,
                                                    VkBuffer     buffer,
                                                    VkDeviceSize offset,
                                                    VkIndexType  type)
    {
        require(offset == 0 && type == VK_INDEX_TYPE_UINT32, "Prepared index ABI");
        boundIndices = buffer;
    }
    VKAPI_ATTR void VKAPI_CALL
    vkCmdDrawIndexed(VkCommandBuffer, uint32_t count, uint32_t, uint32_t first, int32_t offset, uint32_t)
    {
        require(offset == 0, "Already rebased prepared indices");
        drawCount = count;
        drawFirst = first;
    }
}

int main()
try
{
    const auto device   = handle<VkDevice>();
    const auto physical = handle<VkPhysicalDevice>();
    {
        VulkanSkinPaletteBuffer palette(device, physical, 2);
        rejects(
            [&]
            {
                palette.descriptorSet(0);
            });
        GtsSkinPalette source{{glm::mat4(1), glm::translate(glm::mat4(1), glm::vec3(2, 3, 4))}};
        palette.update(0, source);
        const auto set      = palette.descriptorSet(0);
        const auto original = bytes(set);
        require(original.size() == 128 && std::memcmp(original.data(), source.matrices.data(), 128) == 0,
                "Palette bytes retain exact column-major matrices");
        palette.update(1, source);
        const auto otherSet     = palette.descriptorSet(1);
        const auto allocated    = createdBuffers;
        source.matrices[1][3].x = 9;
        palette.update(0, source);
        require(createdBuffers == allocated && bytes(otherSet) == original && bytes(set) != original,
                "Capacity-stable updates reuse allocation without changing other frame slot");
        source.matrices.resize(256, glm::mat4(1));
        palette.update(0, source);
        require(palette.descriptorSet(0) == set && bytes(set).size() == 256 * 64,
                "Growth retains descriptor and correct range");
        const auto prior = bytes(set);
        source.matrices.resize(512, glm::mat4(1));
        failMap = true;
        rejects(
            [&]
            {
                palette.update(0, source);
            });
        failMap = false;
        require(bytes(set) == prior && buffers.size() == 2 && allocations.size() == 2,
                "Failed growth is leak-free and retains prior data");
        source.matrices.resize(1);
        palette.update(0, source);
        require(bytes(set).size() == 64 && palette.matrixCount(0) == 1, "Shrink updates visible range");
        rejects(
            [&]
            {
                palette.update(0, {});
            });
        source.matrices[0][0][0] = std::numeric_limits<float>::infinity();
        rejects(
            [&]
            {
                palette.update(0, source);
            });
        rejects(
            [&]
            {
                palette.update(2, source);
            });
        source.matrices.assign(1025, glm::mat4(1));
        rejects(
            [&]
            {
                palette.update(0, source);
            });

        GtsPreparedSkinnedMesh mesh;
        mesh.vertices.resize(3);
        for (auto& vertex : mesh.vertices)
        {
            vertex.joints  = {1, 0, 0, 0};
            vertex.weights = {1, 0, 0, 0};
        }
        mesh.indices    = {0, 1, 2, 2, 1, 0};
        mesh.primitives = {{0, 3, 7}, {3, 3, 9}};
        VulkanSkinnedMeshResource geometry(device, physical, mesh);
        const auto                command   = handle<VkCommandBuffer>();
        const auto                layout    = handle<VkPipelineLayout>();
        const auto                instances = handle<VkBuffer>();
        rejects(
            [&]
            {
                geometry.drawPrimitive(command, layout, palette, 0, 0, instances);
            });
        geometry.drawPrimitive(command, layout, palette, 1, 1, instances);
        require(boundPalette == otherSet && drawFirst == 3 && drawCount == 3 && boundVertices[1] == instances,
                "Draw binds frame-local palette, object stream and requested primitive range");
        const auto& vertexBytes = allocations.at(buffers.at(boundVertices[0]).memory);
        require(vertexBytes.size() == mesh.vertices.size() * sizeof(GtsSkinnedVertex) &&
                    std::memcmp(vertexBytes.data(), mesh.vertices.data(), vertexBytes.size()) == 0,
                "Geometry upload preserves joints, weights and surface fields byte-for-byte");
        const auto& indexBytes = allocations.at(buffers.at(boundIndices).memory);
        require(std::memcmp(indexBytes.data(), mesh.indices.data(), indexBytes.size()) == 0, "Index bytes unchanged");
        std::array<VkDescriptorSetLayout, 4> sceneLayouts;
        for (auto& entry : sceneLayouts)
            entry = handle<VkDescriptorSetLayout>();
        VulkanPipelineConfig scene;
        scene.fragmentShaderPath = "pbr.spv";
        scene.pushConstantSize   = 64;
        const auto skinned       = makeVulkanSkinnedPipelineConfig(scene, sceneLayouts, palette.layout(), "skin.spv");
        require(skinned.vertexAttributes.back().location == 7 && skinned.vertexBindings[1].stride == 4 &&
                    skinned.descriptorSetLayouts.size() == 5 &&
                    skinned.fragmentShaderPath == scene.fragmentShaderPath && skinned.pushConstantSize == 64 &&
                    scene.vertexAttributes.size() == 5,
                "Separate pipeline preserves material configuration");
    }
    require(buffers.empty() && allocations.empty(), "Resources release all allocations");
    return 0;
}
catch (const std::exception& error)
{
    std::fprintf(stderr, "%s\n", error.what());
    return 1;
}
