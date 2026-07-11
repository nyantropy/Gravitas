#include "GtsModelLoader.hpp"

#include <functional>
#include <sstream>
#include <unordered_map>

namespace
{
    struct ObjVertexKey
    {
        int position = -1;
        int normal = -1;
        int texCoord = -1;

        bool operator==(const ObjVertexKey& other) const
        {
            return position == other.position
                && normal == other.normal
                && texCoord == other.texCoord;
        }
    };

    struct ObjVertexKeyHash
    {
        size_t operator()(const ObjVertexKey& key) const
        {
            size_t value = std::hash<int>()(key.position);
            value ^= std::hash<int>()(key.normal + 0x9e3779b9) + (value << 6u) + (value >> 2u);
            value ^= std::hash<int>()(key.texCoord + 0x7f4a7c15) + (value << 6u) + (value >> 2u);
            return value;
        }
    };

    bool validVecIndex(int index, size_t elementCount)
    {
        return index >= 0 && static_cast<size_t>(index) < elementCount;
    }

    glm::vec3 readPosition(const tinyobj::attrib_t& attrib,
                           int vertexIndex,
                           const std::string& modelPath)
    {
        const size_t count = attrib.vertices.size() / 3u;
        if (!validVecIndex(vertexIndex, count))
        {
            std::ostringstream message;
            message << "OBJ position index out of range in " << modelPath << ": " << vertexIndex;
            throw std::runtime_error(message.str());
        }

        const size_t offset = static_cast<size_t>(vertexIndex) * 3u;
        return {
            attrib.vertices[offset + 0u],
            attrib.vertices[offset + 1u],
            attrib.vertices[offset + 2u]
        };
    }

    glm::vec3 readNormal(const tinyobj::attrib_t& attrib,
                         int normalIndex,
                         const std::string& modelPath)
    {
        if (normalIndex < 0)
            return {0.0f, 0.0f, 1.0f};

        const size_t count = attrib.normals.size() / 3u;
        if (!validVecIndex(normalIndex, count))
        {
            std::ostringstream message;
            message << "OBJ normal index out of range in " << modelPath << ": " << normalIndex;
            throw std::runtime_error(message.str());
        }

        const size_t offset = static_cast<size_t>(normalIndex) * 3u;
        return gts::rendering::safeNormalize(
            {attrib.normals[offset + 0u], attrib.normals[offset + 1u], attrib.normals[offset + 2u]},
            {0.0f, 0.0f, 1.0f});
    }

    glm::vec2 readTexCoord(const tinyobj::attrib_t& attrib,
                           int texCoordIndex,
                           const std::string& modelPath)
    {
        if (texCoordIndex < 0)
            return {0.0f, 0.0f};

        const size_t count = attrib.texcoords.size() / 2u;
        if (!validVecIndex(texCoordIndex, count))
        {
            std::ostringstream message;
            message << "OBJ texture coordinate index out of range in " << modelPath << ": " << texCoordIndex;
            throw std::runtime_error(message.str());
        }

        const size_t offset = static_cast<size_t>(texCoordIndex) * 2u;
        return {
            attrib.texcoords[offset + 0u],
            1.0f - attrib.texcoords[offset + 1u]
        };
    }

    glm::vec4 readColor(const tinyobj::attrib_t& attrib, int vertexIndex)
    {
        const size_t count = attrib.colors.size() / 3u;
        if (!validVecIndex(vertexIndex, count))
            return {1.0f, 1.0f, 1.0f, 1.0f};

        const size_t offset = static_cast<size_t>(vertexIndex) * 3u;
        return {
            attrib.colors[offset + 0u],
            attrib.colors[offset + 1u],
            attrib.colors[offset + 2u],
            1.0f
        };
    }
}

MeshGeometryMetadata GtsModelLoader::loadModel(const std::string& modelPath,
                                               std::vector<Vertex>& vertices,
                                               std::vector<uint32_t>& indices)
{
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn;
    std::string err;

    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, modelPath.c_str()))
        throw std::runtime_error(warn + err + "Path: " + modelPath);

    vertices.clear();
    indices.clear();

    std::unordered_map<ObjVertexKey, uint32_t, ObjVertexKeyHash> uniqueVertices;
    bool allNormalsPresent = true;
    bool allTexCoordsPresent = true;
    for (const tinyobj::shape_t& shape : shapes)
    {
        for (const tinyobj::index_t& index : shape.mesh.indices)
        {
            if (index.vertex_index < 0)
                throw std::runtime_error("OBJ face is missing a position index in " + modelPath);

            ObjVertexKey key{
                index.vertex_index,
                index.normal_index,
                index.texcoord_index
            };

            if (index.normal_index < 0)
                allNormalsPresent = false;
            if (index.texcoord_index < 0)
                allTexCoordsPresent = false;

            auto found = uniqueVertices.find(key);
            if (found == uniqueVertices.end())
            {
                Vertex vertex;
                vertex.pos = readPosition(attrib, index.vertex_index, modelPath);
                vertex.normal = readNormal(attrib, index.normal_index, modelPath);
                vertex.tangent = {1.0f, 0.0f, 0.0f, 1.0f};
                vertex.color = readColor(attrib, index.vertex_index);
                vertex.texCoord = readTexCoord(attrib, index.texcoord_index, modelPath);

                const uint32_t vertexId = static_cast<uint32_t>(vertices.size());
                uniqueVertices.emplace(key, vertexId);
                vertices.push_back(vertex);
                indices.push_back(vertexId);
            }
            else
            {
                indices.push_back(found->second);
            }
        }
    }

    VertexAttributeFlags sourceAttributes =
        VertexAttributeFlags::Position
        | VertexAttributeFlags::Color;
    if (allNormalsPresent && !indices.empty())
        sourceAttributes |= VertexAttributeFlags::Normal;
    if (allTexCoordsPresent && !indices.empty())
        sourceAttributes |= VertexAttributeFlags::UV0;

    return gts::rendering::prepareMeshGeometry(vertices, indices, sourceAttributes);
}
