#include "GtsStaticMeshPreparation.h"

#include <algorithm>
#include <cstddef>
#include <limits>
#include <string>
#include <utility>
#include <variant>

#include "GtsPrimitiveGeometryPreparation.h"
#include "model/domain/model/GtsModelAsset.h"
#include "model/domain/model/GtsModelValidation.h"

namespace
{
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

GtsStaticMeshPreparationResult::GtsStaticMeshPreparationResult(std::optional<GtsPreparedStaticMesh> mesh,
                                                               std::vector<GtsModelDiagnostic>      diagnostics)
    : preparedMesh(std::move(mesh)), messages(std::move(diagnostics))
{
}

bool GtsStaticMeshPreparationResult::hasWarnings() const
{
    return std::any_of(messages.begin(),
                       messages.end(),
                       [](const auto& diagnostic)
                       {
                           return diagnostic.severity == GtsModelDiagnosticSeverity::Warning;
                       });
}

GtsStaticMeshPreparationResult prepareGtsStaticMesh(const GtsModelMesh& mesh)
{
    std::vector<GtsModelDiagnostic> diagnostics;
    size_t                          totalVertices = 0;
    size_t                          totalIndices  = 0;
    const size_t                    maximumCount  = std::numeric_limits<uint32_t>::max();
    for (size_t p = 0; p < mesh.primitives.size(); ++p)
    {
        const auto&       primitive  = mesh.primitives[p];
        const std::string location   = "primitives[" + std::to_string(p) + "]";
        auto              validation = validateGtsModelPrimitive(primitive);
        const bool        valid      = validation.isValid();
        for (auto& diagnostic : validation.diagnostics)
        {
            diagnostic.location.replace(0, std::string("primitive").size(), location);
            diagnostics.push_back(std::move(diagnostic));
        }
        if (primitive.topology != GtsModelPrimitiveTopology::Triangles)
        {
            diagnostics.push_back({GtsModelDiagnosticSeverity::Error,
                                   "STATIC_TOPOLOGY_UNSUPPORTED",
                                   "Static mesh preparation requires triangle lists.",
                                   location});
        }
        for (size_t a = 0; a < primitive.attributes.size(); ++a)
        {
            const auto&       attribute         = primitive.attributes[a];
            const std::string attributeLocation = location + ".attributes[" + std::to_string(a) + "]";
            if (attribute.semantic == GtsVertexSemantic::Joints || attribute.semantic == GtsVertexSemantic::Weights)
            {
                diagnostics.push_back({GtsModelDiagnosticSeverity::Error,
                                       "STATIC_SKINNING_UNSUPPORTED",
                                       "Joint/weight attributes require a different geometry profile.",
                                       attributeLocation});
            }
            else if (attribute.setIndex != 0)
            {
                diagnostics.push_back(
                    {GtsModelDiagnosticSeverity::Warning,
                     "STATIC_ATTRIBUTE_IGNORED",
                     "Static preparation consumes only set 0; this additional stream remains canonical only.",
                     attributeLocation});
            }
        }
        if (!valid)
            continue;
        const size_t vertexCount = positions(primitive).size();
        const size_t indexCount  = primitive.indices.empty() ? vertexCount : primitive.indices.size();
        if (vertexCount > maximumCount - totalVertices || indexCount > maximumCount - totalIndices)
        {
            diagnostics.push_back({GtsModelDiagnosticSeverity::Error,
                                   "STATIC_MESH_TOO_LARGE",
                                   "Combined geometry exceeds the static profile's 32-bit counts.",
                                   location});
        }
        else
        {
            totalVertices += vertexCount;
            totalIndices += indexCount;
        }
    }
    if (std::any_of(diagnostics.begin(),
                    diagnostics.end(),
                    [](const auto& diagnostic)
                    {
                        return diagnostic.severity == GtsModelDiagnosticSeverity::Error;
                    }))
    {
        return {std::nullopt, std::move(diagnostics)};
    }

    GtsPreparedStaticMesh prepared;
    prepared.name = mesh.name;
    prepared.vertices.reserve(totalVertices);
    prepared.indices.reserve(totalIndices);
    prepared.primitives.reserve(mesh.primitives.size());
    for (const auto& primitive : mesh.primitives)
    {
        auto geometry = gtsGeometryPreparationDetail::preparePrimitiveGeometry(primitive);
        const auto& vertices = geometry.vertices;
        const auto& indices = geometry.indices;
        const auto& metadata = geometry.metadata;
        const auto firstVertex = static_cast<uint32_t>(prepared.vertices.size());
        prepared.primitives.push_back({static_cast<uint32_t>(prepared.indices.size()),
                                       static_cast<uint32_t>(indices.size()),
                                       primitive.materialIndex,
                                       metadata});
        prepared.vertices.insert(prepared.vertices.end(), vertices.begin(), vertices.end());
        for (uint32_t index : indices)
            prepared.indices.push_back(firstVertex + index);

        prepared.metadata.attributes =
            prepared.primitives.size() == 1 ? metadata.attributes : prepared.metadata.attributes & metadata.attributes;
        prepared.metadata.generatedNormals |= metadata.generatedNormals;
        prepared.metadata.generatedTangents |= metadata.generatedTangents;
    }
    prepared.metadata.vertexCount = static_cast<uint32_t>(totalVertices);
    prepared.metadata.indexCount  = static_cast<uint32_t>(totalIndices);
    return {std::move(prepared), std::move(diagnostics)};
}
