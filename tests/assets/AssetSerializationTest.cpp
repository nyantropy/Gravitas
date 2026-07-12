#ifdef NDEBUG
#undef NDEBUG
#endif

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "AssetSerializers.h"

namespace
{
    void patchU16(std::vector<uint8_t>& bytes, size_t offset, uint16_t value)
    {
        bytes[offset + 0u] = static_cast<uint8_t>(value & 0xFFu);
        bytes[offset + 1u] = static_cast<uint8_t>((value >> 8u) & 0xFFu);
    }

    void patchU32(std::vector<uint8_t>& bytes, size_t offset, uint32_t value)
    {
        for (uint32_t i = 0; i < 4u; ++i)
            bytes[offset + i] = static_cast<uint8_t>((value >> (i * 8u)) & 0xFFu);
    }

    void patchU64(std::vector<uint8_t>& bytes, size_t offset, uint64_t value)
    {
        for (uint32_t i = 0; i < 8u; ++i)
            bytes[offset + i] = static_cast<uint8_t>((value >> (i * 8u)) & 0xFFull);
    }

    uint64_t readU64(const std::vector<uint8_t>& bytes, size_t offset)
    {
        uint64_t value = 0;
        for (uint32_t i = 0; i < 8u; ++i)
            value |= static_cast<uint64_t>(bytes[offset + i]) << (i * 8u);
        return value;
    }

    gts::rendering::MeshAssetData makeMesh()
    {
        gts::rendering::MeshAssetData mesh;
        mesh.id = 42;
        mesh.debugName = "tri";
        mesh.attributes =
            VertexAttributeFlags::Position
            | VertexAttributeFlags::Normal
            | VertexAttributeFlags::Color
            | VertexAttributeFlags::UV0;
        mesh.vertices = {
            {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f},
             {1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},
            {{1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f},
             {0.0f, 1.0f, 0.0f, 1.0f}, {1.0f, 0.0f}},
            {{0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f},
             {0.0f, 0.0f, 1.0f, 1.0f}, {0.0f, 1.0f}},
        };
        mesh.indices = {0, 1, 2};
        mesh.submeshes.push_back({
            0,
            3,
            gts::rendering::AssetReference::fromLogicalPath("tri_default.gmat"),
            "tri_part"
        });
        mesh.dependencies.push_back(mesh.submeshes.front().material);
        mesh.bounds = {{0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 0.0f}, true};
        return mesh;
    }

    gts::rendering::MaterialAssetData makeMaterial()
    {
        gts::rendering::MaterialAssetData material;
        material.id = 77;
        material.debugName = "mat";
        material.shaderFamily = MaterialShaderFamily::StandardSurface;
        material.baseColor = {0.25f, 0.5f, 0.75f, 0.9f};
        material.metallic = 0.2f;
        material.roughness = 0.6f;
        material.normalScale = 0.8f;
        material.ambientOcclusionStrength = 0.7f;
        material.emissiveFactor = {0.1f, 0.2f, 0.3f};
        material.emissiveStrength = 1.5f;
        material.renderState.alphaMode = MaterialAlphaMode::Blend;
        material.renderState.depthWrite = false;
        material.baseColorTexture = gts::rendering::AssetReference::fromLogicalPath("base.png");
        material.normalTexture = gts::rendering::AssetReference::fromLogicalPath("normal.png");
        material.dependencies = {material.baseColorTexture, material.normalTexture};
        return material;
    }

    void testMeshRoundTrip()
    {
        const gts::rendering::MeshAssetData source = makeMesh();
        std::vector<uint8_t> bytes;
        std::string error;
        assert(gts::rendering::MeshAssetSerializer::serialize(source, bytes, &error));

        gts::rendering::MeshAssetData loaded;
        assert(gts::rendering::MeshAssetSerializer::deserialize(bytes, loaded, &error));
        assert(loaded.id == source.id);
        assert(loaded.debugName == source.debugName);
        assert(loaded.vertices.size() == source.vertices.size());
        assert(loaded.indices == source.indices);
        assert(loaded.submeshes.size() == 1);
        assert(loaded.submeshes.front().material.logicalPath == "tri_default.gmat");
        assert(loaded.dependencies.size() == 1);
        assert(loaded.bounds.valid);
    }

    void testMaterialRoundTrip()
    {
        const gts::rendering::MaterialAssetData source = makeMaterial();
        std::vector<uint8_t> bytes;
        std::string error;
        assert(gts::rendering::MaterialAssetSerializer::serialize(source, bytes, &error));

        gts::rendering::MaterialAssetData loaded;
        assert(gts::rendering::MaterialAssetSerializer::deserialize(bytes, loaded, &error));
        assert(loaded.id == source.id);
        assert(loaded.shaderFamily == source.shaderFamily);
        assert(loaded.baseColor == source.baseColor);
        assert(loaded.metallic == source.metallic);
        assert(loaded.roughness == source.roughness);
        assert(loaded.renderState.alphaMode == source.renderState.alphaMode);
        assert(loaded.baseColorTexture.logicalPath == "base.png");
        assert(loaded.normalTexture.logicalPath == "normal.png");
        assert(loaded.dependencies.size() == 2);
    }

    void testDeterministicOutput()
    {
        const gts::rendering::MeshAssetData mesh = makeMesh();
        std::vector<uint8_t> first;
        std::vector<uint8_t> second;
        assert(gts::rendering::MeshAssetSerializer::serialize(mesh, first));
        assert(gts::rendering::MeshAssetSerializer::serialize(mesh, second));
        assert(first == second);

        const gts::rendering::MaterialAssetData material = makeMaterial();
        assert(gts::rendering::MaterialAssetSerializer::serialize(material, first));
        assert(gts::rendering::MaterialAssetSerializer::serialize(material, second));
        assert(first == second);
    }

    void testMalformedFiles()
    {
        const gts::rendering::MeshAssetData mesh = makeMesh();
        std::vector<uint8_t> bytes;
        std::string error;
        assert(gts::rendering::MeshAssetSerializer::serialize(mesh, bytes, &error));

        gts::rendering::MeshAssetData loaded;
        std::vector<uint8_t> invalid = bytes;
        patchU32(invalid, gts::rendering::CookedAssetMagicOffset, 0);
        assert(!gts::rendering::MeshAssetSerializer::deserialize(invalid, loaded, &error));

        invalid = bytes;
        patchU16(invalid, gts::rendering::CookedAssetVersionOffset, 999);
        assert(!gts::rendering::MeshAssetSerializer::deserialize(invalid, loaded, &error));

        invalid = bytes;
        invalid.resize(gts::rendering::CookedAssetHeaderSize - 1u);
        assert(!gts::rendering::MeshAssetSerializer::deserialize(invalid, loaded, &error));

        invalid = bytes;
        patchU64(invalid, gts::rendering::CookedAssetDependencyTableOffsetOffset, bytes.size() + 128u);
        assert(!gts::rendering::MeshAssetSerializer::deserialize(invalid, loaded, &error));

        invalid = bytes;
        const size_t vertexCountOffset =
            gts::rendering::CookedAssetHeaderSize
            + 8u
            + 4u
            + mesh.debugName.size()
            + 4u
            + 4u
            + 24u;
        patchU32(invalid, vertexCountOffset, gts::rendering::MaxCookedMeshVertices + 1u);
        assert(!gts::rendering::MeshAssetSerializer::deserialize(invalid, loaded, &error));

        invalid = bytes;
        const uint64_t dependencyOffset =
            readU64(invalid, gts::rendering::CookedAssetDependencyTableOffsetOffset);
        patchU32(invalid, static_cast<size_t>(dependencyOffset), 2u);
        assert(!gts::rendering::MeshAssetSerializer::deserialize(invalid, loaded, &error));

        gts::rendering::MeshAssetData badSubmesh = mesh;
        badSubmesh.submeshes.front().firstIndex = 2;
        badSubmesh.submeshes.front().indexCount = 8;
        assert(!gts::rendering::MeshAssetSerializer::serialize(badSubmesh, invalid, &error));
    }
}

int main()
{
    testMeshRoundTrip();
    testMaterialRoundTrip();
    testDeterministicOutput();
    testMalformedFiles();
    std::printf("AssetSerializationTest passed\n");
    return 0;
}
