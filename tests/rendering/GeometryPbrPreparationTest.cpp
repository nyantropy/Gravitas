#include <cmath>
#include <cstdio>
#include <fstream>
#include <string>
#include <type_traits>
#include <vector>

#include <vulkan/vulkan.h>

#include "BitmapFont.h"
#include "DynamicMeshComponent.h"
#include "GlyphLayoutEngine.h"
#include "GtsModelLoader.hpp"
#include "MeshGeometryProcessor.h"
#include "Vertex.h"
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

    bool finiteTangent(const Vertex& vertex)
    {
        return gts::rendering::finiteVec4(vertex.tangent)
            && near(glm::dot(glm::vec3(vertex.tangent), vertex.normal), 0.0f, 0.0005f);
    }

    bool writeTextFile(const std::string& path, const std::string& contents)
    {
        std::ofstream out(path);
        out << contents;
        return out.good();
    }

    bool vertexLayoutMatchesBackendContract()
    {
        const Vertex defaultVertex;
        const auto binding = VulkanVertexDescription::getBindingDescription();
        const auto attributes = VulkanVertexDescription::getAttributeDescriptions();

        return require(std::is_standard_layout_v<Vertex>, "Vertex remains standard layout")
            && require(binding.binding == 0, "Vertex binding is zero")
            && require(binding.stride == sizeof(Vertex), "Vulkan vertex stride matches Vertex")
            && require(binding.inputRate == VK_VERTEX_INPUT_RATE_VERTEX, "Vertex input rate is per-vertex")
            && require(attributes.size() == 5, "Standard vertex exposes five attributes")
            && require(attributes[0].location == 0 && attributes[0].format == VK_FORMAT_R32G32B32_SFLOAT &&
                       attributes[0].offset == offsetof(Vertex, pos), "position attribute matches contract")
            && require(attributes[1].location == 1 && attributes[1].format == VK_FORMAT_R32G32B32_SFLOAT &&
                       attributes[1].offset == offsetof(Vertex, normal), "normal attribute matches contract")
            && require(attributes[2].location == 2 && attributes[2].format == VK_FORMAT_R32G32B32A32_SFLOAT &&
                       attributes[2].offset == offsetof(Vertex, tangent), "tangent attribute matches contract")
            && require(attributes[3].location == 3 && attributes[3].format == VK_FORMAT_R32G32B32A32_SFLOAT &&
                       attributes[3].offset == offsetof(Vertex, color), "color attribute matches contract")
            && require(attributes[4].location == 4 && attributes[4].format == VK_FORMAT_R32G32_SFLOAT &&
                       attributes[4].offset == offsetof(Vertex, texCoord), "UV attribute matches contract")
            && require(nearVec3(defaultVertex.normal, {0.0f, 0.0f, 1.0f}), "default normal is +Z")
            && require(nearVec4(defaultVertex.tangent, {1.0f, 0.0f, 0.0f, 1.0f}), "default tangent is +X with positive handedness")
            && require(nearVec4(defaultVertex.color, {1.0f, 1.0f, 1.0f, 1.0f}), "default color is white")
            && require(near(defaultVertex.texCoord.x, 0.0f) && near(defaultVertex.texCoord.y, 0.0f), "default UV is zero");
    }

    bool normalAndTangentGeneration()
    {
        std::vector<Vertex> vertices = {
            Vertex{{0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f}},
            Vertex{{1.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {1.0f, 0.0f}},
            Vertex{{0.0f, 1.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}}
        };
        const std::vector<uint32_t> indices = {0, 1, 2};

        const MeshGeometryMetadata metadata =
            gts::rendering::prepareMeshGeometry(vertices, indices, UnlitVertexAttributes);

        bool ok = require(metadata.generatedNormals, "missing triangle normals are generated")
            && require(metadata.generatedTangents, "missing triangle tangents are generated")
            && require(hasVertexAttribute(metadata.attributes, VertexAttributeFlags::Normal), "metadata reports normals")
            && require(hasVertexAttribute(metadata.attributes, VertexAttributeFlags::Tangent), "metadata reports tangents");

        for (const Vertex& vertex : vertices)
        {
            ok = require(nearVec3(vertex.normal, {0.0f, 0.0f, 1.0f}), "generated normal is +Z") && ok;
            ok = require(nearVec4(vertex.tangent, {1.0f, 0.0f, 0.0f, 1.0f}), "generated tangent is +X/+handedness") && ok;
        }

        return ok;
    }

    bool degenerateTangentsUseFiniteFallback()
    {
        std::vector<Vertex> vertices = {
            Vertex{{0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f}},
            Vertex{{1.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f}},
            Vertex{{0.0f, 1.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f}}
        };
        const std::vector<uint32_t> indices = {0, 1, 2};

        const MeshGeometryMetadata metadata =
            gts::rendering::prepareMeshGeometry(vertices, indices, UnlitVertexAttributes);

        bool ok = require(metadata.generatedTangents, "degenerate UVs still produce deterministic tangent fallback");
        for (const Vertex& vertex : vertices)
            ok = require(finiteTangent(vertex), "fallback tangent is finite and orthogonal") && ok;

        return ok;
    }

    bool objTupleDeduplicationPreservesSplitNormals()
    {
        const std::string path = "/tmp/gravitas_obj_split_normals.obj";
        const std::string contents =
            "o split\n"
            "v 0 0 0\n"
            "v 1 0 0\n"
            "v 0 1 0\n"
            "vt 0 0\n"
            "vt 1 0\n"
            "vt 0 1\n"
            "vn 0 0 1\n"
            "vn 0 0 -1\n"
            "f 1/1/1 2/2/1 3/3/1\n"
            "f 1/1/2 3/3/2 2/2/2\n";

        if (!require(writeTextFile(path, contents), "test OBJ with split normals can be written"))
            return false;

        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;
        const MeshGeometryMetadata metadata = GtsModelLoader::loadModel(path, vertices, indices);
        std::remove(path.c_str());

        return require(vertices.size() == 6, "OBJ dedupes by position/normal/UV tuple")
            && require(indices.size() == 6, "OBJ indices are preserved")
            && require(!metadata.generatedNormals, "imported split normals are preserved")
            && require(metadata.generatedTangents, "OBJ tangents are generated from imported UVs")
            && require(hasVertexAttribute(metadata.attributes, VertexAttributeFlags::Normal), "OBJ metadata reports imported normals")
            && require(hasVertexAttribute(metadata.attributes, VertexAttributeFlags::UV0), "OBJ metadata reports imported UVs")
            && require(nearVec3(vertices[0].normal, {0.0f, 0.0f, 1.0f}), "first OBJ face uses +Z normal")
            && require(nearVec3(vertices[3].normal, {0.0f, 0.0f, -1.0f}), "second OBJ face uses -Z normal");
    }

    bool objMissingNormalsAreGenerated()
    {
        const std::string path = "/tmp/gravitas_obj_missing_normals.obj";
        const std::string contents =
            "o generated\n"
            "v 0 0 0\n"
            "v 1 0 0\n"
            "v 0 1 0\n"
            "vt 0 0\n"
            "vt 1 0\n"
            "vt 0 1\n"
            "f 1/1 2/2 3/3\n";

        if (!require(writeTextFile(path, contents), "test OBJ without normals can be written"))
            return false;

        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;
        const MeshGeometryMetadata metadata = GtsModelLoader::loadModel(path, vertices, indices);
        std::remove(path.c_str());

        bool ok = require(vertices.size() == 3, "missing-normal OBJ creates expected vertices")
            && require(metadata.generatedNormals, "missing OBJ normals are generated")
            && require(metadata.generatedTangents, "missing OBJ tangents are generated");

        for (const Vertex& vertex : vertices)
            ok = require(nearVec3(vertex.normal, {0.0f, 0.0f, 1.0f}), "generated OBJ normal is +Z") && ok;

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

        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;
        GlyphLayoutEngine::build(text, font, vertices, indices);

        bool ok = require(vertices.size() == 4, "world text emits one quad")
            && require(indices.size() == 6, "world text emits two triangles");

        for (const Vertex& vertex : vertices)
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
    ok = objTupleDeduplicationPreservesSplitNormals() && ok;
    ok = objMissingNormalsAreGenerated() && ok;
    ok = worldTextBuildsSurfaceFrame() && ok;
    ok = dynamicMeshDefaultContractRemainsUnlit() && ok;

    if (!ok)
        return 1;

    std::printf("GeometryPbrPreparationTest passed\n");
    return 0;
}
