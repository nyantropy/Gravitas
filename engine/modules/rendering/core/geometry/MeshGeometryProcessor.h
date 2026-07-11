#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

#include "Vertex.h"

namespace gts::rendering
{
    inline bool finiteVec3(const glm::vec3& value)
    {
        return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
    }

    inline bool finiteVec4(const glm::vec4& value)
    {
        return std::isfinite(value.x)
            && std::isfinite(value.y)
            && std::isfinite(value.z)
            && std::isfinite(value.w);
    }

    inline glm::vec3 safeNormalize(const glm::vec3& value, const glm::vec3& fallback)
    {
        const float lengthSquared = glm::dot(value, value);
        if (lengthSquared <= 1.0e-12f || !std::isfinite(lengthSquared))
            return fallback;

        const glm::vec3 normalized = value * (1.0f / std::sqrt(lengthSquared));
        return finiteVec3(normalized) ? normalized : fallback;
    }

    inline glm::vec3 fallbackTangentForNormal(const glm::vec3& normal)
    {
        const glm::vec3 axis = std::abs(normal.z) < 0.999f
            ? glm::vec3(0.0f, 0.0f, 1.0f)
            : glm::vec3(0.0f, 1.0f, 0.0f);
        return safeNormalize(glm::cross(axis, normal), {1.0f, 0.0f, 0.0f});
    }

    inline void applyVertexDefaults(std::vector<Vertex>& vertices, VertexAttributeFlags sourceAttributes)
    {
        const bool hasNormal = hasVertexAttribute(sourceAttributes, VertexAttributeFlags::Normal);
        const bool hasTangent = hasVertexAttribute(sourceAttributes, VertexAttributeFlags::Tangent);
        const bool hasColor = hasVertexAttribute(sourceAttributes, VertexAttributeFlags::Color);
        const bool hasUv = hasVertexAttribute(sourceAttributes, VertexAttributeFlags::UV0);

        for (Vertex& vertex : vertices)
        {
            if (!hasNormal || !finiteVec3(vertex.normal))
                vertex.normal = {0.0f, 0.0f, 1.0f};
            if (!hasTangent || !finiteVec4(vertex.tangent))
                vertex.tangent = {1.0f, 0.0f, 0.0f, 1.0f};
            if (!hasColor || !finiteVec4(vertex.color))
                vertex.color = {1.0f, 1.0f, 1.0f, 1.0f};
            if (!hasUv || !std::isfinite(vertex.texCoord.x) || !std::isfinite(vertex.texCoord.y))
                vertex.texCoord = {0.0f, 0.0f};
        }
    }

    inline bool generateMissingNormals(std::vector<Vertex>& vertices,
                                       const std::vector<uint32_t>& indices,
                                       VertexAttributeFlags sourceAttributes)
    {
        if (vertices.empty() || indices.size() < 3 ||
            hasVertexAttribute(sourceAttributes, VertexAttributeFlags::Normal))
        {
            return false;
        }

        std::vector<glm::vec3> accumulated(vertices.size(), glm::vec3(0.0f));
        for (size_t i = 0; i + 2 < indices.size(); i += 3)
        {
            const uint32_t ia = indices[i + 0];
            const uint32_t ib = indices[i + 1];
            const uint32_t ic = indices[i + 2];
            if (ia >= vertices.size() || ib >= vertices.size() || ic >= vertices.size())
                continue;

            const glm::vec3 ab = vertices[ib].pos - vertices[ia].pos;
            const glm::vec3 ac = vertices[ic].pos - vertices[ia].pos;
            const glm::vec3 faceNormal = glm::cross(ab, ac);
            const float lengthSquared = glm::dot(faceNormal, faceNormal);
            if (lengthSquared <= 1.0e-12f || !std::isfinite(lengthSquared))
                continue;

            accumulated[ia] += faceNormal;
            accumulated[ib] += faceNormal;
            accumulated[ic] += faceNormal;
        }

        for (size_t i = 0; i < vertices.size(); ++i)
            vertices[i].normal = safeNormalize(accumulated[i], {0.0f, 0.0f, 1.0f});

        return true;
    }

    inline bool generateMissingTangents(std::vector<Vertex>& vertices,
                                        const std::vector<uint32_t>& indices,
                                        VertexAttributeFlags sourceAttributes)
    {
        if (vertices.empty() || indices.size() < 3 ||
            hasVertexAttribute(sourceAttributes, VertexAttributeFlags::Tangent) ||
            !hasVertexAttribute(sourceAttributes, VertexAttributeFlags::UV0))
        {
            return false;
        }

        std::vector<glm::vec3> accumulatedTangent(vertices.size(), glm::vec3(0.0f));
        std::vector<glm::vec3> accumulatedBitangent(vertices.size(), glm::vec3(0.0f));
        for (size_t i = 0; i + 2 < indices.size(); i += 3)
        {
            const uint32_t ia = indices[i + 0];
            const uint32_t ib = indices[i + 1];
            const uint32_t ic = indices[i + 2];
            if (ia >= vertices.size() || ib >= vertices.size() || ic >= vertices.size())
                continue;

            const glm::vec3 p0 = vertices[ia].pos;
            const glm::vec3 p1 = vertices[ib].pos;
            const glm::vec3 p2 = vertices[ic].pos;
            const glm::vec2 uv0 = vertices[ia].texCoord;
            const glm::vec2 uv1 = vertices[ib].texCoord;
            const glm::vec2 uv2 = vertices[ic].texCoord;

            const glm::vec3 edge1 = p1 - p0;
            const glm::vec3 edge2 = p2 - p0;
            const glm::vec2 deltaUv1 = uv1 - uv0;
            const glm::vec2 deltaUv2 = uv2 - uv0;
            const float determinant = deltaUv1.x * deltaUv2.y - deltaUv1.y * deltaUv2.x;
            if (std::abs(determinant) <= 1.0e-8f || !std::isfinite(determinant))
                continue;

            const float invDet = 1.0f / determinant;
            const glm::vec3 tangent =
                (edge1 * deltaUv2.y - edge2 * deltaUv1.y) * invDet;
            const glm::vec3 bitangent =
                (edge2 * deltaUv1.x - edge1 * deltaUv2.x) * invDet;

            if (!finiteVec3(tangent) || !finiteVec3(bitangent))
                continue;

            accumulatedTangent[ia] += tangent;
            accumulatedTangent[ib] += tangent;
            accumulatedTangent[ic] += tangent;
            accumulatedBitangent[ia] += bitangent;
            accumulatedBitangent[ib] += bitangent;
            accumulatedBitangent[ic] += bitangent;
        }

        for (size_t i = 0; i < vertices.size(); ++i)
        {
            const glm::vec3 normal = safeNormalize(vertices[i].normal, {0.0f, 0.0f, 1.0f});
            glm::vec3 tangent = accumulatedTangent[i] - normal * glm::dot(normal, accumulatedTangent[i]);
            tangent = safeNormalize(tangent, fallbackTangentForNormal(normal));

            const glm::vec3 bitangent = accumulatedBitangent[i];
            const float handedness = glm::dot(glm::cross(normal, tangent), bitangent) < 0.0f ? -1.0f : 1.0f;
            vertices[i].normal = normal;
            vertices[i].tangent = {tangent.x, tangent.y, tangent.z, handedness};
        }

        return true;
    }

    inline MeshGeometryMetadata prepareMeshGeometry(std::vector<Vertex>& vertices,
                                                    const std::vector<uint32_t>& indices,
                                                    VertexAttributeFlags sourceAttributes)
    {
        sourceAttributes |= VertexAttributeFlags::Position;
        applyVertexDefaults(vertices, sourceAttributes);

        MeshGeometryMetadata metadata;
        metadata.generatedNormals = generateMissingNormals(vertices, indices, sourceAttributes);
        if (metadata.generatedNormals)
            sourceAttributes |= VertexAttributeFlags::Normal;

        metadata.generatedTangents = generateMissingTangents(vertices, indices, sourceAttributes);
        if (metadata.generatedTangents)
            sourceAttributes |= VertexAttributeFlags::Tangent;

        applyVertexDefaults(vertices, sourceAttributes);
        metadata.attributes = sourceAttributes;
        metadata.vertexCount = static_cast<uint32_t>(vertices.size());
        metadata.indexCount = static_cast<uint32_t>(indices.size());
        return metadata;
    }

    inline MeshGeometryMetadata standardGeneratedMeshMetadata(size_t vertexCount,
                                                             size_t indexCount)
    {
        return MeshGeometryMetadata{
            StandardVertexAttributes,
            static_cast<uint32_t>(vertexCount),
            static_cast<uint32_t>(indexCount),
            false,
            false
        };
    }

}
