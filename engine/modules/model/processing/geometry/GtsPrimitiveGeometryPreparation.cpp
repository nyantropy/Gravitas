#include "GtsPrimitiveGeometryPreparation.h"

#include <algorithm>
#include <numeric>
#include <variant>
#include "assets/geometry/MeshGeometryProcessor.h"
#include "model/domain/model/GtsModelAsset.h"

namespace
{
    template <class T>
    void copyStream(const GtsVertexAttribute& attribute, std::vector<GtsStaticVertex>& vertices, T GtsStaticVertex::* member)
    {
        const auto& values = std::get<std::vector<T>>(attribute.values);
        for (size_t i = 0; i < vertices.size(); ++i)
        {
            vertices[i].*member = values[i];
        }
    }

    const std::vector<glm::vec3>& positions(const GtsModelPrimitive& primitive)
    {
        const auto found =
            std::find_if(primitive.attributes.begin(),
                         primitive.attributes.end(),
                         [](const auto& attribute)
                         {
                             return attribute.semantic == GtsVertexSemantic::Position && attribute.setIndex == 0;
                         });
        return std::get<std::vector<glm::vec3>>(found->values);
    }
} // namespace

gtsGeometryPreparationDetail::PrimitiveGeometry
gtsGeometryPreparationDetail::preparePrimitiveGeometry(const GtsModelPrimitive& primitive)
{
    PrimitiveGeometry prepared;
    auto&             vertices = prepared.vertices;
    vertices.resize(positions(primitive).size());
    auto& indices = prepared.indices;
    indices       = primitive.indices;
    if (indices.empty())
    {
        indices.resize(vertices.size());
        std::iota(indices.begin(), indices.end(), uint32_t{0});
    }
    VertexAttributeFlags          sourceAttributes = VertexAttributeFlags::None;
    const std::vector<glm::vec3>* authoredNormals  = nullptr;
    for (const auto& attribute : primitive.attributes)
    {
        if (attribute.setIndex != 0)
            continue;
        switch (attribute.semantic)
        {
        case GtsVertexSemantic::Position:
            copyStream(attribute, vertices, &GtsStaticVertex::pos);
            sourceAttributes |= VertexAttributeFlags::Position;
            break;
        case GtsVertexSemantic::Normal:
            copyStream(attribute, vertices, &GtsStaticVertex::normal);
            sourceAttributes |= VertexAttributeFlags::Normal;
            authoredNormals = &std::get<std::vector<glm::vec3>>(attribute.values);
            break;
        case GtsVertexSemantic::Tangent:
            copyStream(attribute, vertices, &GtsStaticVertex::tangent);
            sourceAttributes |= VertexAttributeFlags::Tangent;
            break;
        case GtsVertexSemantic::Color:
            copyStream(attribute, vertices, &GtsStaticVertex::color);
            sourceAttributes |= VertexAttributeFlags::Color;
            break;
        case GtsVertexSemantic::TexCoord:
            copyStream(attribute, vertices, &GtsStaticVertex::texCoord);
            sourceAttributes |= VertexAttributeFlags::UV0;
            break;
        case GtsVertexSemantic::Joints:
        case GtsVertexSemantic::Weights:
            break;
        }
    }
    prepared.metadata = gts::rendering::prepareMeshGeometry(vertices, indices, sourceAttributes);
    // tangent generation normalizes working normals
    if (authoredNormals)
    {
        for (size_t i = 0; i < vertices.size(); ++i)
            vertices[i].normal = (*authoredNormals)[i];
    }
    return prepared;
}
