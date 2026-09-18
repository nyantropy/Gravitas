#include "GtsModelCookedLoading.h"

#include <algorithm>
#include <cmath>
#include <string>

#include "MeshAssetLoader.h"
#include "ModelAssetLoader.h"
#include "assets/runtime/RuntimeAssetPolicy.h"

namespace
{
    template <typename Vector> bool finite(const Vector& value)
    {
        for (int i = 0; i < value.length(); ++i)
        {
            if (!std::isfinite(value[i]))
                return false;
        }
        return true;
    }

    bool validPreparedMesh(const gts::rendering::MeshAssetData& mesh, std::string& error)
    {
        // The codec validates byte/count/index/range bounds. Model loading also
        // requires usable finite triangle geometry; no regeneration or repair.
        if (mesh.vertices.empty() || mesh.indices.empty() || mesh.indices.size() % 3 != 0)
        {
            error = "Prepared mesh must contain indexed triangle geometry";
            return false;
        }
        for (const auto& vertex : mesh.vertices)
        {
            if (!finite(vertex.pos) || !finite(vertex.normal) || !finite(vertex.tangent) || !finite(vertex.color) ||
                !finite(vertex.texCoord))
            {
                error = "Prepared mesh contains non-finite vertex data";
                return false;
            }
        }
        for (const auto& primitive : mesh.submeshes)
        {
            if (primitive.firstIndex % 3 != 0 || primitive.indexCount == 0 || primitive.indexCount % 3 != 0)
            {
                error = "Prepared mesh contains a malformed triangle range";
                return false;
            }
        }
        if (mesh.bounds.valid && (!finite(mesh.bounds.min) || !finite(mesh.bounds.max) ||
                                  glm::any(glm::greaterThan(mesh.bounds.min, mesh.bounds.max))))
        {
            error = "Prepared mesh contains invalid bounds";
            return false;
        }
        return true;
    }
} // namespace

std::optional<GtsPreparedModelDefinition> loadGtsCookedModel(const std::filesystem::path&     path,
                                                             std::vector<GtsModelDiagnostic>& diagnostics)
{
    using namespace gts::rendering;
    auto fail = [&](const std::string& message) -> std::optional<GtsPreparedModelDefinition>
    {
        diagnostics.push_back({GtsModelDiagnosticSeverity::Error, "model.cooked.invalid", message, path.string()});
        return {};
    };
    GtsPreparedModelDefinition result;
    result.referenceDirectory = path.parent_path();
    std::string error;
    if (gts::assets::isCookedMeshAssetPath(path))
    {
        MeshAssetData mesh;
        if (!MeshAssetLoader::load(path, mesh, &error) || !validPreparedMesh(mesh, error))
            return fail(error);
        result.dependencies = mesh.dependencies;
        for (const auto& primitive : mesh.submeshes)
        {
            if (!primitive.material.empty())
                result.materials.push_back(primitive.material);
        }
        GtsModelNode node;
        node.name      = mesh.debugName;
        node.meshIndex = 0;
        result.nodes.push_back(std::move(node));
        result.rootNodes.push_back(0);
        result.meshReferences.push_back(AssetReference::fromLogicalPath(path.filename().generic_string()));
        result.meshReferenceDirectories.push_back(path.parent_path());
        result.meshes.push_back(std::move(mesh));
        return result;
    }

    ModelAssetData package;
    if (!ModelAssetLoader::load(path, package, &error))
        return fail(error);
    if (package.nodes.empty())
        return fail("Cooked model contains no nodes");
    result.materials      = package.materials;
    result.dependencies   = package.dependencies;
    result.meshReferences = package.meshes;
    for (const auto& reference : package.meshes)
    {
        if (reference.logicalPath.empty())
            return fail("Mesh reference has no resolvable logical path (ID-only resolution is unavailable)");
        std::error_code pathError;
        const auto meshPath = std::filesystem::weakly_canonical(path.parent_path() / reference.logicalPath, pathError);
        if (pathError)
            return fail("Mesh reference path: " + pathError.message());
        if (!gts::assets::isCookedMeshAssetPath(meshPath))
            return fail("Cooked model mesh reference must identify prepared mesh data: " + reference.logicalPath);
        MeshAssetData mesh;
        if (!MeshAssetLoader::load(meshPath, mesh, &error) || !validPreparedMesh(mesh, error))
        {
            return fail("Mesh reference '" + reference.logicalPath + "': " + error);
        }
        result.meshReferenceDirectories.push_back(meshPath.parent_path());
        result.meshes.push_back(std::move(mesh));
    }
    result.nodes.resize(package.nodes.size());
    for (uint32_t i = 0; i < package.nodes.size(); ++i)
    {
        const auto& source  = package.nodes[i];
        auto&       node    = result.nodes[i];
        node.name           = source.name;
        node.localTransform = source.localTransform;
        for (int column = 0; column < 4; ++column)
        {
            if (!finite(node.localTransform[column]))
                return fail("Node " + std::to_string(i) + " has non-finite transform");
        }
        if (node.localTransform[0][3] != 0 || node.localTransform[1][3] != 0 || node.localTransform[2][3] != 0 ||
            node.localTransform[3][3] != 1)
        {
            return fail("Node " + std::to_string(i) + " transform must be affine");
        }
        if (source.parentIndex < -1 || source.parentIndex >= static_cast<int64_t>(package.nodes.size()))
        {
            return fail("Node " + std::to_string(i) + " has invalid parent");
        }
        if (source.parentIndex == -1)
            result.rootNodes.push_back(i);
        else
            result.nodes[source.parentIndex].children.push_back(i);
        if (!source.mesh.empty())
        {
            auto found = std::find_if(package.meshes.begin(),
                                      package.meshes.end(),
                                      [&](const AssetReference& candidate)
                                      {
                                          return candidate.id == source.mesh.id &&
                                                 candidate.logicalPath == source.mesh.logicalPath;
                                      });
            if (found == package.meshes.end())
                return fail("Node " + std::to_string(i) + " references an unlisted mesh");
            node.meshIndex = static_cast<uint32_t>(found - package.meshes.begin());
        }
    }
    // Parent pointers can be forward references; retain source ordering and reject cycles.
    std::vector<uint8_t> visited(package.nodes.size(), 0);
    for (size_t i = 0; i < package.nodes.size(); ++i)
    {
        int64_t node = static_cast<int64_t>(i);
        while (node >= 0 && visited[node] == 0)
        {
            visited[node] = 1;
            node          = package.nodes[node].parentIndex;
        }
        if (node >= 0 && visited[node] == 1)
            return fail("Cooked model hierarchy contains a cycle");
        node = static_cast<int64_t>(i);
        while (node >= 0 && visited[node] == 1)
        {
            visited[node] = 2;
            node          = package.nodes[node].parentIndex;
        }
    }
    return result;
}
