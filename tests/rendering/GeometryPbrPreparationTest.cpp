#include <cmath>
#include <cstdio>
#include <type_traits>
#include <vector>

#include <vulkan/vulkan.h>

#include "BitmapFont.h"
#include "DynamicMeshComponent.h"
#include "GlyphLayoutEngine.h"
#include "MeshGeometryProcessor.h"
#include "assets/processing/geometry/static/GtsStaticVertex.h"
#include "assets/processing/geometry/GtsGeometryMetadata.h"
#include "VulkanVertexDescription.h"
#include "WorldTextComponent.h"

namespace
{
    bool require(bool condition, const char* message)
    {
        if (condition)
            return true;

        std::fprintf(stderr, "FAIL: %s\n", message);
        return false;
    }

    bool near(float lhs, float rhs, float epsilon = 0.0001f)
    {
        return std::abs(lhs - rhs) <= epsilon;
    }

    bool nearVec3(const glm::vec3& lhs, const glm::vec3& rhs, float epsilon = 0.0001f)
    {
        return near(lhs.x, rhs.x, epsilon)
            && near(lhs.y, rhs.y, epsilon)
            && near(lhs.z, rhs.z, epsilon);
    }

    bool nearVec4(const glm::vec4& lhs, const glm::vec4& rhs, float epsilon = 0.0001f)
    {
        return near(lhs.x, rhs.x, epsilon)
            && near(lhs.y, rhs.y, epsilon)
            && near(lhs.z, rhs.z, epsilon)
            && near(lhs.w, rhs.w, epsilon);
    }

    bool finiteTangent(const GtsStaticVertex& vertex)
    {
        return gts::rendering::finiteVec4(vertex.tangent)
            && near(glm::dot(glm::vec3(vertex.tangent), vertex.normal), 0.0f, 0.0005f);
    }

    bool vertexLayoutMatchesBackendContract()
    {
        static_assert(sizeof(GtsStaticVertex) == 64 && alignof(GtsStaticVertex) == 4);
        static_assert(offsetof(GtsStaticVertex, pos) == 0 && offsetof(GtsStaticVertex, normal) == 12 &&
                      offsetof(GtsStaticVertex, tangent) == 24 && offsetof(GtsStaticVertex, color) == 40 &&
                      offsetof(GtsStaticVertex, texCoord) == 56);
        const GtsStaticVertex defaultVertex;
        const auto binding = VulkanVertexDescription::getBindingDescription();
        const auto attributes = VulkanVertexDescription::getAttributeDescriptions();

        return require(std::is_standard_layout_v<GtsStaticVertex>, "GtsStaticVertex remains standard layout")
            && require(binding.binding == 0, "GtsStaticVertex binding is zero")
            && require(binding.stride == sizeof(GtsStaticVertex), "Vulkan vertex stride matches GtsStaticVertex")
            && require(binding.inputRate == VK_VERTEX_INPUT_RATE_VERTEX, "GtsStaticVertex input rate is per-vertex")
            && require(attributes.size() == 5, "Standard vertex exposes five attributes")
            && require(attributes[0].location == 0 && attributes[0].format == VK_FORMAT_R32G32B32_SFLOAT &&
                       attributes[0].offset == offsetof(GtsStaticVertex, pos), "position attribute matches contract")
            && require(attributes[1].location == 1 && attributes[1].format == VK_FORMAT_R32G32B32_SFLOAT &&
                       attributes[1].offset == offsetof(GtsStaticVertex, normal), "normal attribute matches contract")
            && require(attributes[2].location == 2 && attributes[2].format == VK_FORMAT_R32G32B32A32_SFLOAT &&
                       attributes[2].offset == offsetof(GtsStaticVertex, tangent), "tangent attribute matches contract")
            && require(attributes[3].location == 3 && attributes[3].format == VK_FORMAT_R32G32B32A32_SFLOAT &&
                       attributes[3].offset == offsetof(GtsStaticVertex, color), "color attribute matches contract")
            && require(attributes[4].location == 4 && attributes[4].format == VK_FORMAT_R32G32_SFLOAT &&
                       attributes[4].offset == offsetof(GtsStaticVertex, texCoord), "UV attribute matches contract")
            && require(nearVec3(defaultVertex.normal, {0.0f, 0.0f, 1.0f}), "default normal is +Z")
            && require(nearVec4(defaultVertex.tangent, {1.0f, 0.0f, 0.0f, 1.0f}), "default tangent is +X with positive handedness")
            && require(nearVec4(defaultVertex.color, {1.0f, 1.0f, 1.0f, 1.0f}), "default color is white")
            && require(near(defaultVertex.texCoord.x, 0.0f) && near(defaultVertex.texCoord.y, 0.0f), "default UV is zero");
    }

    bool normalAndTangentGeneration()
    {
        std::vector<GtsStaticVertex> vertices = {
            GtsStaticVertex{.pos = {0.0f, 0.0f, 0.0f}, .color = {1.0f, 1.0f, 1.0f, 1.0f}, .texCoord = {0.0f, 0.0f}},
            GtsStaticVertex{.pos = {1.0f, 0.0f, 0.0f}, .color = {1.0f, 1.0f, 1.0f, 1.0f}, .texCoord = {1.0f, 0.0f}},
            GtsStaticVertex{.pos = {0.0f, 1.0f, 0.0f}, .color = {1.0f, 1.0f, 1.0f, 1.0f}, .texCoord = {0.0f, 1.0f}}
        };
        const std::vector<uint32_t> indices = {0, 1, 2};

        const MeshGeometryMetadata metadata =
            gts::rendering::prepareMeshGeometry(vertices, indices, UnlitVertexAttributes);

        bool ok = require(metadata.generatedNormals, "missing triangle normals are generated")
            && require(metadata.generatedTangents, "missing triangle tangents are generated")
            && require(hasVertexAttribute(metadata.attributes, VertexAttributeFlags::Normal), "metadata reports normals")
            && require(hasVertexAttribute(metadata.attributes, VertexAttributeFlags::Tangent), "metadata reports tangents");

        for (const GtsStaticVertex& vertex : vertices)
        {
            ok = require(nearVec3(vertex.normal, {0.0f, 0.0f, 1.0f}), "generated normal is +Z") && ok;
            ok = require(nearVec4(vertex.tangent, {1.0f, 0.0f, 0.0f, 1.0f}), "generated tangent is +X/+handedness") && ok;
        }

        return ok;
    }

    bool degenerateTangentsUseFiniteFallback()
    {
        std::vector<GtsStaticVertex> vertices = {
            GtsStaticVertex{.pos = {0.0f, 0.0f, 0.0f}, .color = {1.0f, 1.0f, 1.0f, 1.0f}, .texCoord = {0.0f, 0.0f}},
            GtsStaticVertex{.pos = {1.0f, 0.0f, 0.0f}, .color = {1.0f, 1.0f, 1.0f, 1.0f}, .texCoord = {0.0f, 0.0f}},
            GtsStaticVertex{.pos = {0.0f, 1.0f, 0.0f}, .color = {1.0f, 1.0f, 1.0f, 1.0f}, .texCoord = {0.0f, 0.0f}}
        };
        const std::vector<uint32_t> indices = {0, 1, 2};

        const MeshGeometryMetadata metadata =
            gts::rendering::prepareMeshGeometry(vertices, indices, UnlitVertexAttributes);

        bool ok = require(metadata.generatedTangents, "degenerate UVs still produce deterministic tangent fallback");
        for (const GtsStaticVertex& vertex : vertices)
            ok = require(finiteTangent(vertex), "fallback tangent is finite and orthogonal") && ok;

        return ok;
    }

    bool worldTextBuildsSurfaceFrame()
    {
        BitmapFont font;
        font.lineHeight = 10.0f;
        font.glyphs['A'] = GlyphInfo{
            {0.0f, 0.0f},
            {1.0f, 1.0f},
            {8.0f, 8.0f},
            {0.0f, 8.0f},
            8.0f
        };

        WorldTextComponent text;
        text.text = "A";
        text.scale = 1.0f;

        std::vector<GtsStaticVertex> vertices;
        std::vector<uint32_t> indices;
        GlyphLayoutEngine::build(text, font, vertices, indices);

        bool ok = require(vertices.size() == 4, "world text emits one quad")
            && require(indices.size() == 6, "world text emits two triangles");

        for (const GtsStaticVertex& vertex : vertices)
        {
            ok = require(nearVec3(vertex.normal, {0.0f, 0.0f, 1.0f}), "world text normal is +Z") && ok;
            ok = require(nearVec4(vertex.tangent, {1.0f, 0.0f, 0.0f, 1.0f}), "world text tangent is +X/+handedness") && ok;
        }

        return ok;
    }

    bool dynamicMeshDefaultContractRemainsUnlit()
    {
        const DynamicMeshComponent mesh;
        return require(hasVertexAttribute(mesh.sourceAttributes, VertexAttributeFlags::Position), "dynamic mesh defaults include position")
            && require(hasVertexAttribute(mesh.sourceAttributes, VertexAttributeFlags::Color), "dynamic mesh defaults include color")
            && require(hasVertexAttribute(mesh.sourceAttributes, VertexAttributeFlags::UV0), "dynamic mesh defaults include UV")
            && require(!hasVertexAttribute(mesh.sourceAttributes, VertexAttributeFlags::Normal), "dynamic mesh defaults do not claim authored normals")
            && require(!hasVertexAttribute(mesh.sourceAttributes, VertexAttributeFlags::Tangent), "dynamic mesh defaults do not claim authored tangents");
    }
}

int main()
{
    bool ok = true;
    ok = vertexLayoutMatchesBackendContract() && ok;
    ok = normalAndTangentGeneration() && ok;
    ok = degenerateTangentsUseFiniteFallback() && ok;
    ok = worldTextBuildsSurfaceFrame() && ok;
    ok = dynamicMeshDefaultContractRemainsUnlit() && ok;

    if (!ok)
        return 1;

    std::printf("GeometryPbrPreparationTest passed\n");
    return 0;
}
