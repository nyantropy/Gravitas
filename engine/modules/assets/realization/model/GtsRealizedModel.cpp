#include "GtsRealizedModel.h"

#include "assets/serialization/MeshAssetGeometry.h"

namespace
{
    template <class Mesh> std::vector<GtsRealizedPrimitive> canonicalRanges(const Mesh& mesh)
    {
        std::vector<GtsRealizedPrimitive> result;
        for (const auto& range : mesh.primitives)
        {
            GtsRealizedMaterial material;
            if (range.materialIndex)
                material = *range.materialIndex;
            result.push_back({range.firstIndex, range.indexCount, std::move(material)});
        }
        return result;
    }
} // namespace

GtsRealizedGeometry::GtsRealizedGeometry(std::shared_ptr<const GtsPreparedStaticMesh> mesh)
    : storage(mesh), ranges(canonicalRanges(*mesh)), meshBounds(gts::rendering::computeAssetBounds(mesh->vertices))
{
}

GtsRealizedGeometry::GtsRealizedGeometry(std::shared_ptr<const GtsPreparedSkinnedMesh> mesh)
    : storage(mesh), ranges(canonicalRanges(*mesh))
{
    // Stored/bind-space bounds only; animated/conservative bounds are not evaluated here.
    for (const auto& vertex : mesh->vertices)
    {
        if (!meshBounds.valid)
        {
            meshBounds.min = meshBounds.max = vertex.pos;
            meshBounds.valid                = true;
        }
        else
        {
            meshBounds.min = glm::min(meshBounds.min, vertex.pos);
            meshBounds.max = glm::max(meshBounds.max, vertex.pos);
        }
    }
}

GtsRealizedGeometry::GtsRealizedGeometry(std::shared_ptr<const gts::rendering::MeshAssetData> mesh,
                                         const std::filesystem::path&                         referenceDirectory)
    : storage(mesh), meshBounds(mesh->bounds)
{
    for (const auto& range : mesh->submeshes)
    {
        GtsRealizedMaterial material;
        if (!range.material.empty())
            material = GtsExternalMaterialReference{range.material, referenceDirectory};
        ranges.push_back({range.firstIndex, range.indexCount, std::move(material)});
    }
    if (ranges.empty())
        ranges.push_back({0, static_cast<uint32_t>(mesh->indices.size()), {}});
}

GtsGeometryProfile GtsRealizedGeometry::profile() const
{
    return skinnedMesh() ? GtsGeometryProfile::Skinned : GtsGeometryProfile::Static;
}

const GtsPreparedSkinnedMesh* GtsRealizedGeometry::skinnedMesh() const
{
    const auto* mesh = std::get_if<std::shared_ptr<const GtsPreparedSkinnedMesh>>(&storage);
    return mesh ? mesh->get() : nullptr;
}

const GtsPreparedStaticMesh* GtsRealizedGeometry::staticMesh() const
{
    const auto* mesh = std::get_if<std::shared_ptr<const GtsPreparedStaticMesh>>(&storage);
    return mesh ? mesh->get() : nullptr;
}

std::span<const GtsStaticVertex> GtsRealizedGeometry::staticVertices() const
{
    if (const auto* mesh = staticMesh())
        return mesh->vertices;
    if (const auto* mesh = std::get_if<std::shared_ptr<const gts::rendering::MeshAssetData>>(&storage))
        return (*mesh)->vertices;
    return {};
}

std::span<const GtsSkinnedVertex> GtsRealizedGeometry::skinnedVertices() const
{
    if (const auto* mesh = skinnedMesh())
        return mesh->vertices;
    return {};
}

std::span<const uint32_t> GtsRealizedGeometry::indices() const
{
    return std::visit(
        [](const auto& mesh) -> std::span<const uint32_t>
        {
            return mesh->indices;
        },
        storage);
}

MeshGeometryMetadata GtsRealizedGeometry::metadata() const
{
    if (const auto* mesh = staticMesh())
        return mesh->metadata;
    if (const auto* mesh = skinnedMesh())
        return mesh->metadata;
    return gts::rendering::meshMetadataFromAssetData(
        *std::get<std::shared_ptr<const gts::rendering::MeshAssetData>>(storage));
}
