#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <variant>
#include <vector>

#include "model/processing/geometry/static/GtsPreparedStaticMesh.h"
#include "model/processing/geometry/skinned/GtsPreparedSkinnedMesh.h"
#include "assets/serialization/AssetTypes.h"

enum class GtsGeometryProfile
{
    Static,
    Skinned
};

struct GtsExternalMaterialReference
{
    gts::rendering::AssetReference reference;
    std::filesystem::path          referenceDirectory;
};

// Unassigned, canonical model material slot, or unresolved cooked reference.
using GtsRealizedMaterial = std::variant<std::monostate, uint32_t, GtsExternalMaterialReference>;

struct GtsRealizedPrimitive
{
    uint32_t            firstIndex = 0;
    uint32_t            indexCount = 0;
    GtsRealizedMaterial material;
};

// Typed views hide canonical versus cooked backing. Copies retain buffer ownership.
class GtsRealizedGeometry
{
    public:
    explicit GtsRealizedGeometry(std::shared_ptr<const GtsPreparedStaticMesh> mesh);
    explicit GtsRealizedGeometry(std::shared_ptr<const GtsPreparedSkinnedMesh> mesh);
    explicit GtsRealizedGeometry(std::shared_ptr<const gts::rendering::MeshAssetData> mesh,
                                 const std::filesystem::path&                         referenceDirectory);

    GtsGeometryProfile                 profile() const;
    std::span<const GtsStaticVertex>   staticVertices() const;
    std::span<const GtsSkinnedVertex>  skinnedVertices() const;
    std::span<const uint32_t>          indices() const;
    MeshGeometryMetadata               metadata() const;
    const gts::rendering::AssetBounds& bounds() const
    {
        return meshBounds;
    }
    std::span<const GtsRealizedPrimitive> primitives() const
    {
        return ranges;
    }
    // Typed prepared profile access for renderer resource realization.
    const GtsPreparedSkinnedMesh* skinnedMesh() const;
    const GtsPreparedStaticMesh*  staticMesh() const;

    private:
    std::variant<std::shared_ptr<const GtsPreparedStaticMesh>,
                 std::shared_ptr<const GtsPreparedSkinnedMesh>,
                 std::shared_ptr<const gts::rendering::MeshAssetData>>
                                      storage;
    std::vector<GtsRealizedPrimitive> ranges;
    gts::rendering::AssetBounds       meshBounds;
};

