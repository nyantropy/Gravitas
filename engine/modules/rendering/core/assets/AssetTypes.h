#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "GlmConfig.h"
#include "MaterialTypes.h"
#include "TextureColorSpace.h"
#include "Vertex.h"

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

    enum class AssetDependencyType
    {
        SourceFile,
        MaterialLibrary,
        Texture,
        Unknown
    };

    struct AssetDependency
    {
        std::filesystem::path path;
        AssetDependencyType type = AssetDependencyType::Unknown;
        bool required = true;
    };

    struct ImportedTexture
    {
        std::string debugName;
        std::filesystem::path sourcePath;
        TextureColorSpace colorSpace = TextureColorSpace::SRgb;
        MaterialTextureRole intendedRole = MaterialTextureRole::BaseColor;
    };

    struct ImportedMaterial
    {
        std::string name;

        std::optional<glm::vec4> baseColor;
        std::optional<float> metallic;
        std::optional<float> roughness;
        std::optional<float> normalScale;
        std::optional<float> ambientOcclusionStrength;
        std::optional<glm::vec3> emissiveFactor;
        std::optional<float> emissiveStrength;

        std::filesystem::path baseColorTexture;
        std::filesystem::path metallicTexture;
        std::filesystem::path roughnessTexture;
        std::filesystem::path normalTexture;
        std::filesystem::path ambientOcclusionTexture;
        std::filesystem::path emissiveTexture;

        MaterialRenderState renderState{};
        bool vertexColorOnly = false;
    };

    struct ImportedMeshPrimitive
    {
        std::string name;
        uint32_t firstIndex = 0;
        uint32_t indexCount = 0;
        int32_t materialIndex = -1;
        std::string materialName;
    };

    struct ImportedMesh
    {
        std::string debugName;
        std::filesystem::path sourcePath;
        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;
        std::vector<ImportedMeshPrimitive> primitives;
        VertexAttributeFlags sourceAttributes =
            VertexAttributeFlags::Position | VertexAttributeFlags::Color;
        bool hadMissingNormals = false;
        bool hadMissingTexCoords = false;
    };

    struct AssetImportResult
    {
        std::vector<ImportedMesh> meshes;
        std::vector<ImportedMaterial> materials;
        std::vector<ImportedTexture> textures;
        std::vector<AssetDependency> dependencies;
        std::vector<AssetDiagnostic> diagnostics;

        bool hasErrors() const
        {
            for (const AssetDiagnostic& diagnostic : diagnostics)
            {
                if (diagnostic.severity == AssetDiagnosticSeverity::Error)
                    return true;
            }
            return false;
        }

        bool succeeded() const
        {
            return !hasErrors();
        }
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
        std::vector<Vertex> vertices;
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
