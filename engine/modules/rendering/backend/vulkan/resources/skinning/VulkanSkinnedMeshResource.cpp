#include "VulkanSkinnedMeshResource.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include "VulkanSkinningBuffer.h"
#include "VulkanSkinPaletteBuffer.h"

void VulkanSkinnedMeshResource::validate(const GtsPreparedSkinnedMesh& mesh)
{
    if (mesh.vertices.empty() || mesh.indices.empty() || mesh.primitives.empty() ||
        mesh.vertices.size() > std::numeric_limits<uint32_t>::max() ||
        mesh.indices.size() > std::numeric_limits<uint32_t>::max())
        throw std::invalid_argument("Skinned GPU geometry requires nonempty 32-bit indexed primitive ranges");
    for (size_t i = 0; i < mesh.vertices.size(); ++i)
    {
        const auto& vertex = mesh.vertices[i];
        const auto  finite = [](const auto& value)
        {
            for (glm::length_t c = 0; c < value.length(); ++c)
                if (!std::isfinite(value[c]))
                    return false;
            return true;
        };
        if (!finite(vertex.pos) || !finite(vertex.normal) || !finite(vertex.tangent) || !finite(vertex.color) ||
            !finite(vertex.texCoord) || !finite(vertex.weights))
            throw std::invalid_argument("Non-finite skinned vertex " + std::to_string(i));
        double total = 0;
        for (int c = 0; c < 4; ++c)
        {
            if (vertex.weights[c] < 0 || vertex.joints[c] == std::numeric_limits<uint32_t>::max())
                throw std::invalid_argument("Invalid skinned influence at vertex " + std::to_string(i));
            total += vertex.weights[c];
        }
        if (std::abs(total - 1.0) > 1e-4)
            throw std::invalid_argument("Skinned GPU weights must sum to one");
    }
    for (auto index : mesh.indices)
        if (index >= mesh.vertices.size())
            throw std::invalid_argument("Skinned GPU index out of range");
    uint64_t end = 0;
    for (const auto& range : mesh.primitives)
    {
        if (range.firstIndex != end || !range.indexCount || range.indexCount % 3 != 0)
            throw std::invalid_argument("Invalid skinned triangle primitive range");
        end += range.indexCount;
        if (end > mesh.indices.size())
            throw std::invalid_argument("Skinned primitive range exceeds indices");
    }
    if (end != mesh.indices.size())
        throw std::invalid_argument("Skinned primitive ranges must cover indices");
}

VulkanSkinnedMeshResource::VulkanSkinnedMeshResource(VkDevice                      device,
                                                     VkPhysicalDevice              physicalDevice,
                                                     const GtsPreparedSkinnedMesh& mesh)
    : ranges(mesh.primitives)
{
    validate(mesh);
    for (const auto& vertex : mesh.vertices)
        for (int c = 0; c < 4; ++c)
            paletteSize = std::max(paletteSize, vertex.joints[c] + 1);
    const VkDeviceSize vertexBytes = mesh.vertices.size() * VkDeviceSize(sizeof(GtsSkinnedVertex));
    const VkDeviceSize indexBytes  = mesh.indices.size() * VkDeviceSize(sizeof(uint32_t));
    vertices =
        std::make_unique<VulkanSkinningBuffer>(device, physicalDevice, vertexBytes, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    indices =
        std::make_unique<VulkanSkinningBuffer>(device, physicalDevice, indexBytes, VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
    vertices->write(mesh.vertices.data(), vertexBytes);
    indices->write(mesh.indices.data(), indexBytes);
}

VulkanSkinnedMeshResource::~VulkanSkinnedMeshResource() = default;

void VulkanSkinnedMeshResource::drawPrimitive(VkCommandBuffer                commandBuffer,
                                              VkPipelineLayout               layout,
                                              const VulkanSkinPaletteBuffer& palette,
                                              uint32_t                       frame,
                                              uint32_t                       primitive,
                                              VkBuffer                       instanceObjects,
                                              uint32_t                       instanceCount,
                                              uint32_t                       firstInstance) const
{
    const auto& range = ranges.at(primitive);
    if (palette.matrixCount(frame) < paletteSize)
        throw std::invalid_argument("Skinned draw palette is smaller than vertex skin-local slot range");
    if (!commandBuffer || !layout || !instanceObjects || !instanceCount)
        throw std::invalid_argument("Invalid skinned draw context");
    const auto set = palette.descriptorSet(frame);
    vkCmdBindDescriptorSets(commandBuffer,
                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                            layout,
                            VulkanSkinPaletteBuffer::DescriptorSet,
                            1,
                            &set,
                            0,
                            nullptr);
    const VkBuffer     vertexBuffers[] = {vertices->handle(), instanceObjects};
    const VkDeviceSize offsets[]       = {0, 0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 2, vertexBuffers, offsets);
    vkCmdBindIndexBuffer(commandBuffer, indices->handle(), 0, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(commandBuffer, range.indexCount, instanceCount, range.firstIndex, 0, firstInstance);
}
