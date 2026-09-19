#include "GtsSkinnedMeshPreparation.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <utility>
#include <variant>

#include "GtsPrimitiveGeometryPreparation.h"
#include "model/domain/model/GtsModelAsset.h"
#include "model/domain/model/GtsModelSkinValidation.h"
#include "model/domain/model/GtsModelValidation.h"
#include "model/domain/skin/GtsSkinBinding.h"
#include "model/domain/skin/GtsSkinBindingValidation.h"

namespace
{
    struct InfluenceSet
    {
        const std::vector<glm::uvec4>* joints  = nullptr;
        const std::vector<glm::vec4>*  weights = nullptr;
    };

    struct Influence
    {
        uint32_t slot;
        float    weight;
    };

    template <class Vector> bool finite(const Vector& value)
    {
        for (glm::length_t i = 0; i < value.length(); ++i)
            if (!std::isfinite(value[i]))
                return false;
        return true;
    }
} // namespace

GtsSkinnedMeshPreparationResult::GtsSkinnedMeshPreparationResult(std::optional<GtsPreparedSkinnedMesh> mesh,
                                                                 std::vector<GtsModelDiagnostic>       diagnostics)
    : preparedMesh(std::move(mesh)), messages(std::move(diagnostics))
{
}

bool GtsSkinnedMeshPreparationResult::hasWarnings() const
{
    return std::any_of(messages.begin(),
                       messages.end(),
                       [](const auto& diagnostic)
                       {
                           return diagnostic.severity == GtsModelDiagnosticSeverity::Warning;
                       });
}

GtsSkinnedMeshPreparationResult prepareGtsSkinnedMesh(const GtsModelMesh& mesh, const GtsSkinBinding& binding)
{
    std::vector<GtsModelDiagnostic> diagnostics;
    const auto                      error = [&](const char* code, std::string message, std::string location)
    {
        diagnostics.push_back({GtsModelDiagnosticSeverity::Error, code, std::move(message), std::move(location)});
    };
    for (const auto& diagnostic : validateGtsSkinBinding(binding).diagnostics)
        error(diagnostic.code.c_str(), diagnostic.message, "binding." + diagnostic.location);
    for (size_t slot = 0; slot < binding.joints.size(); ++slot)
        if (binding.joints[slot].skeletonNodeIndex >= binding.targetSkeletonCompatibility.nodes().size())
            error("SKINNED_BINDING_NODE_OUT_OF_RANGE",
                  "Binding mapping exceeds its required skeleton contract.",
                  "binding.joints[" + std::to_string(slot) + "]");

    size_t       totalVertices = 0;
    size_t       totalIndices  = 0;
    const size_t maximumCount  = std::numeric_limits<uint32_t>::max();
    for (size_t p = 0; p < mesh.primitives.size(); ++p)
    {
        const auto& primitive  = mesh.primitives[p];
        const auto  location   = "primitives[" + std::to_string(p) + "]";
        auto        validation = validateGtsModelPrimitiveSkin(primitive, binding);
        const bool  valid      = validation.isValid();
        for (auto& diagnostic : validation.diagnostics)
        {
            diagnostic.location.replace(0, std::string("primitive").size(), location);
            diagnostics.push_back(std::move(diagnostic));
        }
        if (primitive.topology != GtsModelPrimitiveTopology::Triangles)
            error("SKINNED_TOPOLOGY_UNSUPPORTED", "Skinned preparation requires triangle lists.", location);
        size_t vertexCount = 0;
        for (size_t a = 0; a < primitive.attributes.size(); ++a)
        {
            const auto& attribute = primitive.attributes[a];
            if (attribute.semantic == GtsVertexSemantic::Position && attribute.setIndex == 0 && valid)
                vertexCount = std::get<std::vector<glm::vec3>>(attribute.values).size();
            if (attribute.setIndex != 0 && attribute.semantic != GtsVertexSemantic::Joints &&
                attribute.semantic != GtsVertexSemantic::Weights)
                diagnostics.push_back({GtsModelDiagnosticSeverity::Warning,
                                       "SKINNED_ATTRIBUTE_IGNORED",
                                       "Skinned profile consumes only set 0 of non-influence attributes.",
                                       location + ".attributes[" + std::to_string(a) + "]"});
        }
        if (!valid)
            continue;
        const size_t indexCount = primitive.indices.empty() ? vertexCount : primitive.indices.size();
        if (vertexCount > maximumCount - totalVertices || indexCount > maximumCount - totalIndices)
            error("SKINNED_MESH_TOO_LARGE", "Combined geometry exceeds the profile's 32-bit counts.", location);
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
        return {std::nullopt, std::move(diagnostics)};

    GtsPreparedSkinnedMesh prepared;
    prepared.name = mesh.name;
    prepared.vertices.reserve(totalVertices);
    prepared.indices.reserve(totalIndices);
    prepared.primitives.reserve(mesh.primitives.size());
    for (size_t p = 0; p < mesh.primitives.size(); ++p)
    {
        const auto&                      primitive = mesh.primitives[p];
        auto                             geometry  = gtsGeometryPreparationDetail::preparePrimitiveGeometry(primitive);
        std::map<uint32_t, InfluenceSet> sets;
        for (const auto& attribute : primitive.attributes)
        {
            if (attribute.semantic == GtsVertexSemantic::Joints)
                sets[attribute.setIndex].joints = &std::get<std::vector<glm::uvec4>>(attribute.values);
            else if (attribute.semantic == GtsVertexSemantic::Weights)
                sets[attribute.setIndex].weights = &std::get<std::vector<glm::vec4>>(attribute.values);
        }
        const auto                  firstVertex = static_cast<uint32_t>(prepared.vertices.size());
        GtsPreparedSkinnedPrimitive range;
        range.firstIndex    = static_cast<uint32_t>(prepared.indices.size());
        range.indexCount    = static_cast<uint32_t>(geometry.indices.size());
        range.materialIndex = primitive.materialIndex;
        range.metadata      = geometry.metadata;
        std::vector<Influence> influences;
        for (size_t v = 0; v < geometry.vertices.size(); ++v)
        {
            influences.clear();
            // Numeric set order then XYZW component order is the canonical tie order.
            for (const auto& [index, set] : sets)
                for (glm::length_t component = 0; component < 4; ++component)
                    if ((*set.weights)[v][component] > 0)
                        influences.push_back({(*set.joints)[v][component], (*set.weights)[v][component]});
            range.influences.maxSourceInfluenceCount =
                std::max(range.influences.maxSourceInfluenceCount, influences.size());
            if (influences.size() > 4)
            {
                std::stable_sort(influences.begin(),
                                 influences.end(),
                                 [](const auto& a, const auto& b)
                                 {
                                     return a.weight > b.weight;
                                 });
                influences.resize(4);
                ++range.influences.reducedInfluenceVertices;
            }
            double selectedTotal = 0;
            for (const auto& influence : influences)
                selectedTotal += influence.weight;
            if (!(selectedTotal > 0) || !std::isfinite(selectedTotal))
            {
                error("SKINNED_WEIGHT_TOTAL",
                      "Selected weights must have a finite positive sum.",
                      "primitives[" + std::to_string(p) + "].vertices[" + std::to_string(v) + "]");
                return {std::nullopt, std::move(diagnostics)};
            }
            const auto&      source = geometry.vertices[v];
            GtsSkinnedVertex vertex{source.pos, source.normal, source.tangent, source.color, source.texCoord};
            for (size_t i = 0; i < influences.size(); ++i)
            {
                vertex.joints[static_cast<glm::length_t>(i)] = influences[i].slot;
                vertex.weights[static_cast<glm::length_t>(i)] =
                    static_cast<float>(influences[i].weight / selectedTotal);
            }
            if (!finite(vertex.pos) || !finite(vertex.normal) || !finite(vertex.tangent) || !finite(vertex.color) ||
                !finite(vertex.texCoord) || !finite(vertex.weights))
            {
                error("SKINNED_VERTEX_NONFINITE",
                      "Prepared vertex components must be finite.",
                      "primitives[" + std::to_string(p) + "].vertices[" + std::to_string(v) + "]");
                return {std::nullopt, std::move(diagnostics)};
            }
            prepared.vertices.push_back(vertex);
        }
        if (range.influences.reducedInfluenceVertices != 0)
            diagnostics.push_back({GtsModelDiagnosticSeverity::Warning,
                                   "SKINNED_INFLUENCES_REDUCED",
                                   "Reduced " + std::to_string(range.influences.reducedInfluenceVertices) +
                                       " vertices to their four strongest influences and renormalized weights.",
                                   "primitives[" + std::to_string(p) + "]"});
        for (uint32_t index : geometry.indices)
            prepared.indices.push_back(firstVertex + index);
        prepared.metadata.attributes =
            p == 0 ? range.metadata.attributes : prepared.metadata.attributes & range.metadata.attributes;
        prepared.metadata.generatedNormals |= range.metadata.generatedNormals;
        prepared.metadata.generatedTangents |= range.metadata.generatedTangents;
        prepared.influences.reducedInfluenceVertices += range.influences.reducedInfluenceVertices;
        prepared.influences.maxSourceInfluenceCount =
            std::max(prepared.influences.maxSourceInfluenceCount, range.influences.maxSourceInfluenceCount);
        prepared.primitives.push_back(range);
    }
    prepared.metadata.vertexCount = static_cast<uint32_t>(totalVertices);
    prepared.metadata.indexCount  = static_cast<uint32_t>(totalIndices);
    return {std::move(prepared), std::move(diagnostics)};
}
