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

    gts::rendering::ModelAssetData makeModel()
    {
        gts::rendering::ModelAssetData model;
        model.id = 99;
        model.debugName = "robot";
        model.meshes = {
            gts::rendering::AssetReference::fromLogicalPath("robot_body.gmesh"),
            gts::rendering::AssetReference::fromLogicalPath("robot_head.gmesh")
        };
        model.materials = {
            gts::rendering::AssetReference::fromLogicalPath("robot_body.gmat"),
            gts::rendering::AssetReference::fromLogicalPath("robot_head.gmat")
        };

        gts::rendering::ModelNodeAssetData body;
        body.name = "body";
        body.mesh = model.meshes[0];
        body.localTransform[3][0] = 1.0f;
        body.localTransform[3][1] = 2.0f;
        body.localTransform[3][2] = 3.0f;

        gts::rendering::ModelNodeAssetData head;
        head.name = "head";
        head.parentIndex = 0;
        head.mesh = model.meshes[1];
        head.localTransform[0][0] = 0.5f;
        head.localTransform[1][1] = 0.5f;
        head.localTransform[2][2] = 0.5f;

        model.nodes = {body, head};
        model.dependencies = {
            model.meshes[0],
            model.meshes[1],
            model.materials[0],
            model.materials[1]
        };
        return model;
    }

    gts::rendering::TextureAssetData makeTexture()
    {
        gts::rendering::TextureAssetData texture;
        texture.id = 123;
        texture.debugName = "checker";
        texture.width = 2;
        texture.height = 2;
        texture.mipCount = 2;
        texture.format = gts::rendering::TextureAssetFormat::RGBA8_SRgb;
        texture.colorSpace = TextureColorSpace::SRgb;
        texture.defaultSampler.minFilter = gts::rendering::TextureFilter::Linear;
        texture.defaultSampler.magFilter = gts::rendering::TextureFilter::Linear;
        texture.defaultSampler.mipmapMode = gts::rendering::TextureMipmapMode::Linear;
        texture.defaultSampler.addressU = gts::rendering::TextureAddressMode::Repeat;
        texture.defaultSampler.addressV = gts::rendering::TextureAddressMode::Repeat;
        texture.defaultSampler.addressW = gts::rendering::TextureAddressMode::Repeat;
        texture.defaultSampler.maxAnisotropy = 1.0f;
        texture.mips.push_back({
            2,
            2,
            8,
            16,
            {
                255, 0, 0, 255,
                0, 255, 0, 255,
                0, 0, 255, 255,
                255, 255, 255, 255
            }
        });
        texture.mips.push_back({
            1,
            1,
            4,
            4,
            {128, 128, 128, 255}
        });
        texture.dependencies.push_back(
            gts::rendering::AssetReference::fromLogicalPath("checker.png"));
        return texture;
    }

    size_t textureMipCountOffset(const gts::rendering::TextureAssetData& texture)
    {
        return gts::rendering::CookedAssetHeaderSize
            + 8u
            + 4u
            + texture.debugName.size()
            + 8u;
    }

    size_t textureFormatOffset(const gts::rendering::TextureAssetData& texture)
    {
        return textureMipCountOffset(texture) + 4u;
    }

    size_t textureColorSpaceOffset(const gts::rendering::TextureAssetData& texture)
    {
        return textureFormatOffset(texture) + 4u;
    }

    size_t textureFirstMipDescriptorOffset(const gts::rendering::TextureAssetData& texture)
    {
        return gts::rendering::CookedAssetHeaderSize
            + 8u
            + 4u
            + texture.debugName.size()
            + 12u
            + 8u
            + 24u
            + 4u;
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

    void testModelRoundTrip()
    {
        const gts::rendering::ModelAssetData source = makeModel();
        std::vector<uint8_t> bytes;
        std::string error;
        assert(gts::rendering::ModelAssetSerializer::serialize(source, bytes, &error));

        gts::rendering::ModelAssetData loaded;
        assert(gts::rendering::ModelAssetSerializer::deserialize(bytes, loaded, &error));
        assert(loaded.id == source.id);
        assert(loaded.debugName == source.debugName);
        assert(loaded.meshes.size() == 2);
        assert(loaded.materials.size() == 2);
        assert(loaded.nodes.size() == 2);
        assert(loaded.nodes[0].name == "body");
        assert(loaded.nodes[0].parentIndex == -1);
        assert(loaded.nodes[0].mesh.logicalPath == "robot_body.gmesh");
        assert(loaded.nodes[0].localTransform[3][0] == 1.0f);
        assert(loaded.nodes[1].parentIndex == 0);
        assert(loaded.nodes[1].mesh.logicalPath == "robot_head.gmesh");
        assert(loaded.nodes[1].localTransform[0][0] == 0.5f);
        assert(loaded.dependencies.size() == 4);
    }

    void testTextureRoundTrip()
    {
        const gts::rendering::TextureAssetData source = makeTexture();
        std::vector<uint8_t> bytes;
        std::string error;
        assert(gts::rendering::TextureAssetSerializer::serialize(source, bytes, &error));

        gts::rendering::TextureAssetData loaded;
        assert(gts::rendering::TextureAssetSerializer::deserialize(bytes, loaded, &error));
        assert(loaded.id == source.id);
        assert(loaded.debugName == source.debugName);
        assert(loaded.width == 2);
        assert(loaded.height == 2);
        assert(loaded.mipCount == 2);
        assert(loaded.format == gts::rendering::TextureAssetFormat::RGBA8_SRgb);
        assert(loaded.colorSpace == TextureColorSpace::SRgb);
        assert(loaded.defaultSampler.addressU == gts::rendering::TextureAddressMode::Repeat);
        assert(loaded.mips.size() == 2);
        assert(loaded.mips[0].bytes == source.mips[0].bytes);
        assert(loaded.mips[1].width == 1);
        assert(loaded.dependencies.size() == 1);
        assert(loaded.dependencies[0].logicalPath == "checker.png");
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

        const gts::rendering::ModelAssetData model = makeModel();
        assert(gts::rendering::ModelAssetSerializer::serialize(model, first));
        assert(gts::rendering::ModelAssetSerializer::serialize(model, second));
        assert(first == second);

        const gts::rendering::TextureAssetData texture = makeTexture();
        assert(gts::rendering::TextureAssetSerializer::serialize(texture, first));
        assert(gts::rendering::TextureAssetSerializer::serialize(texture, second));
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

        const gts::rendering::ModelAssetData model = makeModel();
        assert(gts::rendering::ModelAssetSerializer::serialize(model, bytes, &error));

        gts::rendering::ModelAssetData loadedModel;
        invalid = bytes;
        const size_t nodeCountOffset =
            gts::rendering::CookedAssetHeaderSize
            + 8u
            + 4u
            + model.debugName.size();
        patchU32(invalid, nodeCountOffset, gts::rendering::MaxCookedModelNodes + 1u);
        assert(!gts::rendering::ModelAssetSerializer::deserialize(invalid, loadedModel, &error));

        gts::rendering::ModelAssetData badParent = model;
        badParent.nodes[0].parentIndex = 99;
        assert(!gts::rendering::ModelAssetSerializer::serialize(badParent, invalid, &error));

        const gts::rendering::TextureAssetData texture = makeTexture();
        assert(gts::rendering::TextureAssetSerializer::serialize(texture, bytes, &error));

        gts::rendering::TextureAssetData loadedTexture;
        invalid = bytes;
        patchU32(invalid, gts::rendering::CookedAssetMagicOffset, 0);
        assert(!gts::rendering::TextureAssetSerializer::deserialize(invalid, loadedTexture, &error));

        invalid = bytes;
        patchU16(invalid, gts::rendering::CookedAssetVersionOffset, 999);
        assert(!gts::rendering::TextureAssetSerializer::deserialize(invalid, loadedTexture, &error));

        invalid = bytes;
        invalid.resize(gts::rendering::CookedAssetHeaderSize - 1u);
        assert(!gts::rendering::TextureAssetSerializer::deserialize(invalid, loadedTexture, &error));

        invalid = bytes;
        patchU32(invalid, textureMipCountOffset(texture), gts::rendering::MaxCookedTextureMipLevels + 1u);
        assert(!gts::rendering::TextureAssetSerializer::deserialize(invalid, loadedTexture, &error));

        invalid = bytes;
        patchU32(invalid, textureFormatOffset(texture), 999u);
        assert(!gts::rendering::TextureAssetSerializer::deserialize(invalid, loadedTexture, &error));

        invalid = bytes;
        patchU32(invalid, textureColorSpaceOffset(texture), 999u);
        assert(!gts::rendering::TextureAssetSerializer::deserialize(invalid, loadedTexture, &error));

        const size_t firstMipDescriptor = textureFirstMipDescriptorOffset(texture);
        invalid = bytes;
        patchU32(invalid, firstMipDescriptor, 4u);
        assert(!gts::rendering::TextureAssetSerializer::deserialize(invalid, loadedTexture, &error));

        invalid = bytes;
        patchU64(invalid, firstMipDescriptor + 16u, bytes.size() + 128u);
        assert(!gts::rendering::TextureAssetSerializer::deserialize(invalid, loadedTexture, &error));

        invalid = bytes;
        patchU64(invalid, firstMipDescriptor + 16u, gts::rendering::CookedAssetHeaderSize);
        assert(!gts::rendering::TextureAssetSerializer::deserialize(invalid, loadedTexture, &error));

        invalid = bytes;
        const uint64_t firstMipOffset = readU64(invalid, firstMipDescriptor + 16u);
        const size_t secondMipDescriptor = firstMipDescriptor + 32u;
        patchU64(invalid, secondMipDescriptor + 16u, firstMipOffset);
        assert(!gts::rendering::TextureAssetSerializer::deserialize(invalid, loadedTexture, &error));

        invalid = bytes;
        const uint64_t textureDependencyOffset =
            readU64(invalid, gts::rendering::CookedAssetDependencyTableOffsetOffset);
        patchU32(invalid, static_cast<size_t>(textureDependencyOffset), 2u);
        assert(!gts::rendering::TextureAssetSerializer::deserialize(invalid, loadedTexture, &error));

        gts::rendering::TextureAssetData badMipRange = texture;
        badMipRange.mips[1].width = 2;
        assert(!gts::rendering::TextureAssetSerializer::serialize(badMipRange, invalid, &error));
    }
}

int main()
{
    testMeshRoundTrip();
    testMaterialRoundTrip();
    testModelRoundTrip();
    testTextureRoundTrip();
    testDeterministicOutput();
    testMalformedFiles();
    std::printf("AssetSerializationTest passed\n");
    return 0;
}
