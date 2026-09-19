#include "GtsStaticModelRealization.h"

#include <algorithm>
#include <limits>

#include "assets/serialization/MeshAssetGeometry.h"
#include "model/domain/model/GtsModelAsset.h"
#include "model/domain/model/GtsModelValidation.h"
#include "model/processing/geometry/static/GtsStaticMeshPreparation.h"

namespace gts::rendering
{
std::optional<MeshAssetData> realizeGtsFlatStaticModel(
    const GtsModelAsset& model,
    std::vector<GtsModelDiagnostic>& diagnostics,
    const std::vector<AssetReference>& materials,
    const AssetReference& defaultMaterial)
{
    const auto validation = validateGtsModelAsset(model);
    diagnostics.insert(diagnostics.end(), validation.diagnostics.begin(), validation.diagnostics.end());
    if (!validation.isValid())
        return std::nullopt;

    auto fail = [&](const char* code, const char* message) -> std::optional<MeshAssetData>
    {
        diagnostics.push_back({GtsModelDiagnosticSeverity::Error, code, message, {}});
        return std::nullopt;
    };
    if (model.meshes.empty() || model.nodes.size() != model.meshes.size())
        return fail("STATIC_MODEL_NOT_FLAT", "Single-mesh realization requires one identity root per mesh");
    std::vector<bool> referenced(model.meshes.size(), false);
    for (const auto& node : model.nodes)
    {
        if (!node.children.empty() || node.localTransform != glm::mat4(1.0f) ||
            !node.meshIndex || referenced[*node.meshIndex])
            return fail("STATIC_MODEL_NOT_FLAT", "Hierarchy, transforms, and instancing require model realization");
        referenced[*node.meshIndex] = true;
    }
    if (!materials.empty() && materials.size() < model.materials.size())
        return fail("STATIC_MATERIAL_REFERENCES", "Cooked material reference table is incomplete");

    MeshAssetData result;
    for (const auto& mesh : model.meshes)
    {
        const auto preparation = prepareGtsStaticMesh(mesh);
        diagnostics.insert(diagnostics.end(), preparation.diagnostics().begin(), preparation.diagnostics().end());
        if (!preparation.succeeded())
            return std::nullopt;
        const auto& prepared = *preparation.mesh();
        const auto limit = std::numeric_limits<uint32_t>::max();
        if (prepared.vertices.size() > limit - result.vertices.size() ||
            prepared.indices.size() > limit - result.indices.size())
            return fail("STATIC_MESH_TOO_LARGE", "Combined mesh exceeds uint32 vertex/index capacity");
        const auto vertexOffset = static_cast<uint32_t>(result.vertices.size());
        const auto indexOffset = static_cast<uint32_t>(result.indices.size());
        if (!prepared.vertices.empty())
        {
            result.attributes = result.vertices.empty() ? prepared.metadata.attributes
                : result.attributes & prepared.metadata.attributes;
        }
        result.vertices.insert(result.vertices.end(), prepared.vertices.begin(), prepared.vertices.end());
        for (const auto index : prepared.indices)
            result.indices.push_back(vertexOffset + index);
        for (const auto& primitive : prepared.primitives)
        {
            const auto material = primitive.materialIndex && !materials.empty()
                ? materials[*primitive.materialIndex] : defaultMaterial;
            result.submeshes.push_back({indexOffset + primitive.firstIndex, primitive.indexCount, material, mesh.name});
            if (!material.empty() && std::none_of(result.dependencies.begin(), result.dependencies.end(),
                [&](const auto& existing) { return existing.id == material.id && existing.logicalPath == material.logicalPath; }))
                result.dependencies.push_back(material);
        }
        result.generatedNormals |= prepared.metadata.generatedNormals;
        result.generatedTangents |= prepared.metadata.generatedTangents;
    }
    if (result.indices.empty())
        return fail("STATIC_MODEL_EMPTY", "Single-mesh realization requires renderable geometry");
    result.bounds = computeAssetBounds(result.vertices);
    return result;
}
}
