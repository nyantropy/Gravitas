#include <cmath>
#include <cstddef>
#include <cstring>
#include <cstdio>
#include <limits>
#include <stdexcept>
#include <type_traits>

#include "VulkanSkinnedVertexDescription.h"
#include "VulkanStaticVertexDescription.h"
#include "VulkanSkinPaletteBuffer.h"
#include "VulkanSkinnedMeshResource.h"
#include "VulkanSkinnedPipelineConfig.h"

namespace
{
    void require(bool value, const char* message)
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
        catch (const std::invalid_argument&)
        {
            return;
        }
        throw std::runtime_error("Malformed GPU input accepted");
    }
} // namespace

int main()
try
{
    static_assert(std::is_standard_layout_v<GtsSkinnedVertex> && std::is_aggregate_v<GtsSkinnedVertex>);
    static_assert(sizeof(GtsSkinnedVertex) == 96 && alignof(GtsSkinnedVertex) == 4);
    static_assert(offsetof(GtsSkinnedVertex, pos) == 0 && offsetof(GtsSkinnedVertex, normal) == 12);
    static_assert(offsetof(GtsSkinnedVertex, tangent) == 24 && offsetof(GtsSkinnedVertex, color) == 40);
    static_assert(offsetof(GtsSkinnedVertex, texCoord) == 56 && offsetof(GtsSkinnedVertex, joints) == 64);
    static_assert(offsetof(GtsSkinnedVertex, weights) == 80);
    const auto     stat      = VulkanStaticVertexDescription::getAttributeDescriptions();
    const auto     skin      = VulkanSkinnedVertexDescription::getAttributeDescriptions();
    const uint32_t offsets[] = {0, 12, 24, 40, 56, 64, 80};
    for (uint32_t i = 0; i < 7; ++i)
    {
        require(skin[i].location == i && skin[i].binding == 0 && skin[i].offset == offsets[i], "Skinned vertex ABI");
        if (i < 5)
            require(skin[i].format == stat[i].format && skin[i].offset == stat[i].offset &&
                        skin[i].location == stat[i].location,
                    "Common attribute ABI unchanged");
    }
    require(skin[5].format == VK_FORMAT_R32G32B32A32_UINT && skin[6].format == VK_FORMAT_R32G32B32A32_SFLOAT,
            "Integer slots and floating weights");
    require(VulkanSkinnedVertexDescription::getBindingDescription().stride == 96 &&
                VulkanStaticVertexDescription::getBindingDescription().stride == 64,
            "Independent strides");
    const auto binding = VulkanSkinPaletteBuffer::layoutBinding();
    require(binding.binding == 0 && binding.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER &&
                binding.stageFlags == VK_SHADER_STAGE_VERTEX_BIT && binding.descriptorCount == 1 &&
                VulkanSkinPaletteBuffer::DescriptorSet == 4,
            "Palette descriptor ABI");
    static_assert(sizeof(glm::mat4) == 64 && sizeof(glm::mat4::col_type) == 16);
    glm::mat4 matrix(1);
    matrix[3] = {2, 3, 4, 1};
    float matrixBytes[16];
    std::memcpy(matrixBytes, &matrix, sizeof(matrix));
    require(matrixBytes[12] == 2 && matrixBytes[14] == 4, "Column-major translation upload");

    VulkanPipelineConfig staticConfig;
    rejects(
        [&]
        {
            makeVulkanSkinnedPipelineConfig(staticConfig, {}, VK_NULL_HANDLE, "skin.spv");
        });
    require(staticConfig.vertexAttributes.size() == 5 && staticConfig.descriptorSetLayouts.empty(),
            "Static defaults unchanged");

    GtsPreparedSkinnedMesh mesh;
    mesh.vertices.resize(3);
    for (auto& vertex : mesh.vertices)
        vertex.weights = {1, 0, 0, 0};
    mesh.indices = {0, 1, 2};
    mesh.primitives.push_back({0, 3, 7});
    VulkanSkinnedMeshResource::validate(mesh);
    auto bad                  = mesh;
    bad.vertices[0].weights.x = 0;
    rejects(
        [&]
        {
            VulkanSkinnedMeshResource::validate(bad);
        });
    bad                   = mesh;
    bad.vertices[0].pos.x = std::numeric_limits<float>::infinity();
    rejects(
        [&]
        {
            VulkanSkinnedMeshResource::validate(bad);
        });
    bad            = mesh;
    bad.indices[0] = 3;
    rejects(
        [&]
        {
            VulkanSkinnedMeshResource::validate(bad);
        });
    bad                          = mesh;
    bad.primitives[0].firstIndex = 1;
    rejects(
        [&]
        {
            VulkanSkinnedMeshResource::validate(bad);
        });
    bad                      = mesh;
    bad.vertices[0].joints.w = std::numeric_limits<uint32_t>::max();
    rejects(
        [&]
        {
            VulkanSkinnedMeshResource::validate(bad);
        });
    require(mesh.vertices[0].weights.x == 1 && mesh.primitives[0].materialIndex == 7, "Validation preserves input");
    return 0;
}
catch (const std::exception& error)
{
    std::fprintf(stderr, "%s\n", error.what());
    return 1;
}
