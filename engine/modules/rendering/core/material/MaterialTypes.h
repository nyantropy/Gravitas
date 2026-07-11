#pragma once

#include <cstdint>
#include <string>
#include <utility>

#include "GlmConfig.h"
#include "Types.h"

struct MaterialDefinitionHandle
{
    uint32_t id = 0;
    uint32_t generation = 0;

    bool valid() const
    {
        return id != 0;
    }
};

struct MaterialInstanceHandle
{
    uint32_t id = 0;
    uint32_t generation = 0;

    bool valid() const
    {
        return id != 0;
    }
};

struct MaterialGpuHandle
{
    uint32_t id = 0;
    uint32_t generation = 0;

    bool valid() const
    {
        return id != 0;
    }
};

inline bool operator==(MaterialDefinitionHandle lhs, MaterialDefinitionHandle rhs)
{
    return lhs.id == rhs.id && lhs.generation == rhs.generation;
}

inline bool operator!=(MaterialDefinitionHandle lhs, MaterialDefinitionHandle rhs)
{
    return !(lhs == rhs);
}

inline bool operator==(MaterialInstanceHandle lhs, MaterialInstanceHandle rhs)
{
    return lhs.id == rhs.id && lhs.generation == rhs.generation;
}

inline bool operator!=(MaterialInstanceHandle lhs, MaterialInstanceHandle rhs)
{
    return !(lhs == rhs);
}

inline bool operator==(MaterialGpuHandle lhs, MaterialGpuHandle rhs)
{
    return lhs.id == rhs.id && lhs.generation == rhs.generation;
}

inline bool operator!=(MaterialGpuHandle lhs, MaterialGpuHandle rhs)
{
    return !(lhs == rhs);
}

enum class MaterialBlendMode
{
    Alpha,
    Additive
};

enum class MaterialShaderFamily
{
    LegacyUnlit,
    StandardSurface
};

enum class MaterialAlphaMode
{
    Opaque,
    Mask,
    Blend
};

enum class MaterialTextureSource
{
    None,
    AssetPath,
    ResolvedTexture
};

struct MaterialRenderState
{
    MaterialAlphaMode alphaMode = MaterialAlphaMode::Opaque;
    float alphaCutoff = 0.5f;
    bool doubleSided = false;
    bool depthWrite = true;

    // Temporary legacy compatibility until render commands stop selecting
    // alpha/additive pipelines directly.
    MaterialBlendMode legacyBlendMode = MaterialBlendMode::Alpha;
};

struct MaterialDefinition
{
    MaterialShaderFamily shaderFamily = MaterialShaderFamily::LegacyUnlit;
};

struct MaterialTextureBinding
{
    MaterialTextureSource source = MaterialTextureSource::AssetPath;
    std::string path;
    texture_id_type resolvedTextureID = 0;

    static MaterialTextureBinding assetPath(std::string texturePath)
    {
        MaterialTextureBinding binding;
        binding.source = MaterialTextureSource::AssetPath;
        binding.path = std::move(texturePath);
        return binding;
    }

    static MaterialTextureBinding resolved(texture_id_type textureID)
    {
        MaterialTextureBinding binding;
        binding.source = MaterialTextureSource::ResolvedTexture;
        binding.resolvedTextureID = textureID;
        return binding;
    }
};

struct MaterialInstance
{
    MaterialDefinitionHandle definition;
    glm::vec4 baseColor = {1.0f, 1.0f, 1.0f, 1.0f};
    MaterialTextureBinding baseColorTexture = MaterialTextureBinding::assetPath({});
    MaterialRenderState renderState{};
    bool vertexColorOnly = false;
    uint64_t version = 1;
};

struct MaterialVariantKey
{
    uint64_t value = 0;
};

inline bool operator==(MaterialVariantKey lhs, MaterialVariantKey rhs)
{
    return lhs.value == rhs.value;
}

inline bool operator!=(MaterialVariantKey lhs, MaterialVariantKey rhs)
{
    return !(lhs == rhs);
}

struct MaterialGpuState
{
    MaterialGpuHandle gpuHandle;
    MaterialInstanceHandle instance;
    uint64_t uploadedVersion = 0;
    texture_id_type baseColorTextureID = 0;
    std::string boundTexturePath;
    glm::vec4 baseColor = {1.0f, 1.0f, 1.0f, 1.0f};
    MaterialRenderState renderState{};
    bool vertexColorOnly = false;
    MaterialVariantKey variantKey{};
};

struct MaterialSyncResult
{
    MaterialGpuState* state = nullptr;
    bool changed = false;
    bool parameterChanged = false;
    bool topologyChanged = false;
};

inline MaterialVariantKey makeMaterialVariantKey(const MaterialInstance& instance)
{
    const uint64_t alphaMode = static_cast<uint64_t>(instance.renderState.alphaMode) & 0x3ull;
    const uint64_t blendMode = static_cast<uint64_t>(instance.renderState.legacyBlendMode) & 0x3ull;
    const uint64_t depth = instance.renderState.depthWrite ? 1ull : 0ull;
    const uint64_t sidedness = instance.renderState.doubleSided ? 1ull : 0ull;
    const uint64_t vertexColor = instance.vertexColorOnly ? 1ull : 0ull;
    return MaterialVariantKey{
        (alphaMode << 5)
        | (blendMode << 3)
        | (depth << 2)
        | (sidedness << 1)
        | vertexColor
    };
}

inline MaterialAlphaMode alphaModeForLegacyMaterial(MaterialBlendMode blendMode,
                                                    float alpha,
                                                    bool depthWrite)
{
    if (blendMode == MaterialBlendMode::Additive)
        return MaterialAlphaMode::Blend;

    if (!depthWrite || alpha < 1.0f)
        return MaterialAlphaMode::Blend;

    return MaterialAlphaMode::Opaque;
}

namespace std
{
    template<>
    struct hash<MaterialInstanceHandle>
    {
        size_t operator()(MaterialInstanceHandle handle) const noexcept
        {
            return (static_cast<size_t>(handle.id) << 32u) ^ static_cast<size_t>(handle.generation);
        }
    };

    template<>
    struct hash<MaterialDefinitionHandle>
    {
        size_t operator()(MaterialDefinitionHandle handle) const noexcept
        {
            return (static_cast<size_t>(handle.id) << 32u) ^ static_cast<size_t>(handle.generation);
        }
    };
}
