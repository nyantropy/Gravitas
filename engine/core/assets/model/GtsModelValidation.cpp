#include "GtsModelValidation.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <set>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "GtsModelAsset.h"

namespace
{
    void addError(GtsModelValidationResult& result, std::string code,
                  std::string message, std::string location)
    {
        result.diagnostics.push_back({GtsModelDiagnosticSeverity::Error,
                                      std::move(code), std::move(message), std::move(location)});
    }

    bool hasCanonicalType(const GtsVertexAttribute& attribute)
    {
        switch (attribute.semantic)
        {
        case GtsVertexSemantic::Position:
        case GtsVertexSemantic::Normal:
            return std::holds_alternative<std::vector<glm::vec3>>(attribute.values);
        case GtsVertexSemantic::TexCoord:
            return std::holds_alternative<std::vector<glm::vec2>>(attribute.values);
        case GtsVertexSemantic::Tangent:
        case GtsVertexSemantic::Color:
        case GtsVertexSemantic::Weights:
            return std::holds_alternative<std::vector<glm::vec4>>(attribute.values);
        case GtsVertexSemantic::Joints:
            return std::holds_alternative<std::vector<glm::uvec4>>(attribute.values);
        }
        return false;
    }

    void validatePrimitive(const GtsModelPrimitive& primitive,
                           const std::string& location, GtsModelValidationResult& result)
    {
        size_t vertexCount = 0;
        for (const GtsVertexAttribute& attribute : primitive.attributes)
        {
            if (attribute.semantic == GtsVertexSemantic::Position && attribute.setIndex == 0)
            {
                if (const auto* positions = std::get_if<std::vector<glm::vec3>>(&attribute.values))
                {
                    vertexCount = positions->size();
                }
                break;
            }
        }
        if (vertexCount == 0)
        {
            addError(result, "MODEL_POSITION_REQUIRED", "A primitive requires nonempty Position[0] vec3 data.", location);
        }

        std::set<std::pair<GtsVertexSemantic, uint32_t>> keys;
        for (size_t i = 0; i < primitive.attributes.size(); ++i)
        {
            const GtsVertexAttribute& attribute = primitive.attributes[i];
            const std::string attributeLocation = location + ".attributes[" + std::to_string(i) + "]";
            if (!keys.emplace(attribute.semantic, attribute.setIndex).second)
            {
                addError(result, "MODEL_ATTRIBUTE_DUPLICATE", "Semantic and set index must be unique.", attributeLocation);
            }
            if (!hasCanonicalType(attribute))
            {
                addError(result, "MODEL_ATTRIBUTE_TYPE", "Attribute does not have its semantic's canonical type.", attributeLocation);
                continue;
            }
            std::visit([&](const auto& values)
            {
                if (values.empty() || values.size() != vertexCount)
                {
                    addError(result, "MODEL_ATTRIBUTE_COUNT", "Present streams must be nonempty and match Position[0].", attributeLocation);
                }
                for (const auto& value : values)
                {
                    for (glm::length_t component = 0; component < value.length(); ++component)
                    {
                        if (!std::isfinite(value[component]))
                        {
                            addError(result, "MODEL_ATTRIBUTE_NONFINITE", "Attribute contains a nonfinite value.", attributeLocation);
                            return;
                        }
                    }
                }
            }, attribute.values);
        }

        for (uint32_t index : primitive.indices)
        {
            if (index >= vertexCount)
            {
                addError(result, "MODEL_INDEX_OUT_OF_RANGE", "Index does not reference a vertex.", location);
                break;
            }
        }

        const size_t elementCount = primitive.indices.empty() ? vertexCount : primitive.indices.size();
        size_t groupSize = 1;
        switch (primitive.topology)
        {
        case GtsModelPrimitiveTopology::Points: break;
        case GtsModelPrimitiveTopology::Lines: groupSize = 2; break;
        case GtsModelPrimitiveTopology::Triangles: groupSize = 3; break;
        default:
            addError(result, "MODEL_TOPOLOGY_INVALID", "Unknown primitive topology.", location);
            return;
        }
        if (elementCount % groupSize != 0)
        {
            addError(result, "MODEL_TOPOLOGY_COUNT", "Element count does not form complete primitive groups.", location);
        }
    }
}

GtsModelValidationResult validateGtsModelPrimitive(const GtsModelPrimitive& primitive)
{
    GtsModelValidationResult result;
    validatePrimitive(primitive, "primitive", result);
    return result;
}

GtsModelValidationResult validateGtsModelAsset(const GtsModelAsset& asset)
{
    GtsModelValidationResult result;
    for (size_t meshIndex = 0; meshIndex < asset.meshes.size(); ++meshIndex)
    {
        const GtsModelMesh& mesh = asset.meshes[meshIndex];
        for (size_t primitiveIndex = 0; primitiveIndex < mesh.primitives.size(); ++primitiveIndex)
        {
            const GtsModelPrimitive& primitive = mesh.primitives[primitiveIndex];
            const std::string location = "meshes[" + std::to_string(meshIndex)
                + "].primitives[" + std::to_string(primitiveIndex) + "]";
            validatePrimitive(primitive, location, result);
            if (primitive.materialIndex && *primitive.materialIndex >= asset.materials.size())
            {
                addError(result, "MODEL_MATERIAL_OUT_OF_RANGE", "Material index does not reference a model material.", location);
            }
        }
    }

    std::vector<size_t> parentCounts(asset.nodes.size(), 0);
    for (size_t nodeIndex = 0; nodeIndex < asset.nodes.size(); ++nodeIndex)
    {
        const GtsModelNode& node = asset.nodes[nodeIndex];
        const std::string location = "nodes[" + std::to_string(nodeIndex) + "]";
        if (node.meshIndex && *node.meshIndex >= asset.meshes.size())
        {
            addError(result, "MODEL_MESH_OUT_OF_RANGE", "Mesh index does not reference a model mesh.", location);
        }
        bool finiteTransform = true;
        for (glm::length_t column = 0; column < 4; ++column)
        {
            for (glm::length_t row = 0; row < 4; ++row)
            {
                finiteTransform = std::isfinite(node.localTransform[column][row]) && finiteTransform;
            }
        }
        if (!finiteTransform)
        {
            addError(result, "MODEL_TRANSFORM_NONFINITE", "Local transform contains a nonfinite value.", location);
        }
        for (uint32_t child : node.children)
        {
            if (child >= asset.nodes.size())
            {
                addError(result, "MODEL_CHILD_OUT_OF_RANGE", "Child index does not reference a model node.", location);
                continue;
            }
            if (++parentCounts[child] > 1)
            {
                addError(result, "MODEL_NODE_MULTIPLE_PARENTS", "A node must have only one incoming child reference.", location);
            }
        }
    }

    std::vector<bool> roots(asset.nodes.size(), false);
    std::vector<uint32_t> pending;
    for (uint32_t root : asset.rootNodes)
    {
        if (root >= asset.nodes.size())
        {
            addError(result, "MODEL_ROOT_OUT_OF_RANGE", "Root index does not reference a model node.", "rootNodes");
            continue;
        }
        if (roots[root])
        {
            addError(result, "MODEL_ROOT_DUPLICATE", "Root indices must be unique.", "rootNodes");
        }
        if (parentCounts[root] != 0)
        {
            addError(result, "MODEL_ROOT_HAS_PARENT", "A root must not also be a child.", "rootNodes");
        }
        roots[root] = true;
        pending.push_back(root);
    }

    // iterative traversal also handles deeply nested or cyclic malformed input
    std::vector<bool> reached(asset.nodes.size(), false);
    while (!pending.empty())
    {
        const uint32_t nodeIndex = pending.back();
        pending.pop_back();
        if (reached[nodeIndex])
        {
            continue;
        }
        reached[nodeIndex] = true;
        for (uint32_t child : asset.nodes[nodeIndex].children)
        {
            if (child < asset.nodes.size())
            {
                pending.push_back(child);
            }
        }
    }
    for (size_t nodeIndex = 0; nodeIndex < asset.nodes.size(); ++nodeIndex)
    {
        if (!reached[nodeIndex])
        {
            addError(result, "MODEL_NODE_UNREACHABLE", "Node is unreachable from roots (missing root or hierarchy cycle).",
                     "nodes[" + std::to_string(nodeIndex) + "]");
        }
    }
    return result;
}
