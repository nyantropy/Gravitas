#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "GlmConfig.h"
#include "AssetMaterialTypes.h"
#include "TextureColorSpace.h"
#include "GtsStaticVertex.h"
#include "GtsGeometryMetadata.h"

namespace gts::rendering
{
    using AssetId = uint64_t;
    inline constexpr AssetId InvalidAssetId = 0;

    inline AssetId stableAssetIdFromLogicalPath(std::string_view logicalPath)
    {
        uint64_t hash = 14695981039346656037ull;
        for (char ch : logicalPath)
        {
            hash ^= static_cast<unsigned char>(ch);
            hash *= 1099511628211ull;
        }
        return hash == InvalidAssetId ? 1ull : hash;
    }

    struct AssetReference
    {
        AssetId id = InvalidAssetId;
        std::string logicalPath;

        bool empty() const
        {
            return id == InvalidAssetId && logicalPath.empty();
        }

        static AssetReference fromLogicalPath(std::string path)
        {
            AssetReference reference;
            reference.logicalPath = std::move(path);
            reference.id = reference.logicalPath.empty()
                ? InvalidAssetId
                : stableAssetIdFromLogicalPath(reference.logicalPath);
            return reference;
        }
    };

    enum class AssetDiagnosticSeverity
    {
        Info,
        Warning,
        Error
    };

    struct AssetDiagnostic
    {
        AssetDiagnosticSeverity severity = AssetDiagnosticSeverity::Info;
        std::string code;
        std::string message;
        std::filesystem::path sourcePath;
        uint32_t line = 0;
    };

    enum class TextureAssetFormat : uint16_t
    {
        R8 = 1,
        RG8 = 2,
        RGBA8_UNorm = 3,
        RGBA8_SRgb = 4,
        RGBA16_Float = 5,
        BC1_UNorm = 100,
        BC1_SRgb = 101,
        BC3_UNorm = 102,
        BC3_SRgb = 103,
        BC5_UNorm = 104,
        BC7_UNorm = 105,
        BC7_SRgb = 106
    };

    enum class TextureFilter : uint16_t
    {
        Nearest = 1,
        Linear = 2
    };

    enum class TextureMipmapMode : uint16_t
    {
        Nearest = 1,
        Linear = 2
    };

    enum class TextureAddressMode : uint16_t
    {
        Repeat = 1,
        ClampToEdge = 2
    };

    enum class TextureCookRole : uint16_t
    {
        BaseColor = 1,
        MetallicRoughness = 2,
        Normal = 3,
        AmbientOcclusion = 4,
        Emissive = 5,
        UiColor = 6,
        FontAtlas = 7,
        ParticleColor = 8,
        Data = 9
    };

    struct TextureMipData
    {
        uint32_t width = 0;
        uint32_t height = 0;
        uint32_t rowPitch = 0;
        uint32_t slicePitch = 0;
        std::vector<uint8_t> bytes;
    };

    struct TextureSamplerDesc
    {
        TextureFilter minFilter = TextureFilter::Linear;
        TextureFilter magFilter = TextureFilter::Linear;
        TextureMipmapMode mipmapMode = TextureMipmapMode::Linear;
        TextureAddressMode addressU = TextureAddressMode::Repeat;
        TextureAddressMode addressV = TextureAddressMode::Repeat;
        TextureAddressMode addressW = TextureAddressMode::Repeat;
        float maxAnisotropy = 1.0f;
    };

    struct TextureAssetData
    {
        AssetId id = InvalidAssetId;
        std::string debugName;
        uint32_t width = 0;
        uint32_t height = 0;
        uint32_t mipCount = 0;
        TextureAssetFormat format = TextureAssetFormat::RGBA8_SRgb;
        TextureColorSpace colorSpace = TextureColorSpace::SRgb;
        TextureSamplerDesc defaultSampler;
        std::vector<TextureMipData> mips;
        std::vector<AssetReference> dependencies;
    };

    struct AssetBounds
    {
        glm::vec3 min = {0.0f, 0.0f, 0.0f};
        glm::vec3 max = {0.0f, 0.0f, 0.0f};
        bool valid = false;
    };

    struct SubmeshAssetData
    {
        uint32_t firstIndex = 0;
        uint32_t indexCount = 0;
        AssetReference material;
        std::string debugName;
    };

    struct MeshAssetData
    {
        AssetId id = InvalidAssetId;
        std::string debugName;
        VertexAttributeFlags attributes = VertexAttributeFlags::None;
        std::vector<GtsStaticVertex> vertices;
        std::vector<uint32_t> indices;
        std::vector<SubmeshAssetData> submeshes;
        std::vector<AssetReference> dependencies;
        AssetBounds bounds;
        bool generatedNormals = false;
        bool generatedTangents = false;
    };

    struct MaterialAssetData
    {
        AssetId id = InvalidAssetId;
        std::string debugName;

        MaterialShaderFamily shaderFamily = MaterialShaderFamily::Unlit;

        glm::vec4 baseColor = {1.0f, 1.0f, 1.0f, 1.0f};
        float metallic = 0.0f;
        float roughness = 1.0f;
        float normalScale = 1.0f;
        float ambientOcclusionStrength = 1.0f;

        glm::vec3 emissiveFactor = {0.0f, 0.0f, 0.0f};
        float emissiveStrength = 1.0f;

        AssetReference baseColorTexture;
        AssetReference metallicRoughnessTexture;
        AssetReference normalTexture;
        AssetReference ambientOcclusionTexture;
        AssetReference emissiveTexture;
        std::vector<AssetReference> dependencies;

        MaterialRenderState renderState{};
        bool vertexColorOnly = false;
    };

}
