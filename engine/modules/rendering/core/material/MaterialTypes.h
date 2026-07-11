#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

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

enum class RenderQueue
{
    Opaque,
    AlphaMasked,
    Transparent
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

struct MaterialFrameState
{
    MaterialInstanceHandle instance;
    MaterialGpuHandle gpuHandle;
    MaterialVariantKey variantKey{};
    RenderQueue renderQueue = RenderQueue::Opaque;
    uint64_t uploadedVersion = 0;

    // Temporary Vulkan compatibility data. These fields are material-owned
    // cache state; render commands and object uploads must not duplicate them.
    texture_id_type baseColorTextureID = 0;
    glm::vec4 baseColor = {1.0f, 1.0f, 1.0f, 1.0f};
    MaterialRenderState renderState{};
    bool vertexColorOnly = false;
};

struct MaterialFrameData
{
    std::vector<MaterialFrameState> materials;

    void clear()
    {
        materials.clear();
    }

    const MaterialFrameState* find(MaterialGpuHandle handle) const
    {
        for (const MaterialFrameState& material : materials)
        {
            if (material.gpuHandle == handle)
                return &material;
        }
        return nullptr;
    }

    void upsert(const MaterialFrameState& material)
    {
        for (MaterialFrameState& existing : materials)
        {
            if (existing.gpuHandle == material.gpuHandle)
            {
                existing = material;
                return;
            }
        }

        materials.push_back(material);
    }
};

inline RenderQueue renderQueueForAlphaMode(MaterialAlphaMode alphaMode)
{
    switch (alphaMode)
    {
        case MaterialAlphaMode::Opaque: return RenderQueue::Opaque;
        case MaterialAlphaMode::Mask:   return RenderQueue::AlphaMasked;
        case MaterialAlphaMode::Blend:  return RenderQueue::Transparent;
    }
    return RenderQueue::Opaque;
}

inline MaterialVariantKey makeMaterialVariantKey(MaterialShaderFamily shaderFamily,
                                                 const MaterialInstance& instance)
{
    const uint64_t shader = static_cast<uint64_t>(shaderFamily) & 0xFFull;
    const uint64_t alphaMode = static_cast<uint64_t>(instance.renderState.alphaMode) & 0x3ull;
    // Temporary legacy blend mode bit until additive/alpha compatibility
    // becomes a proper backend material realization in the PBR pipeline.
    const uint64_t blendMode = static_cast<uint64_t>(instance.renderState.legacyBlendMode) & 0x3ull;
    const uint64_t depth = instance.renderState.depthWrite ? 1ull : 0ull;
    const uint64_t sidedness = instance.renderState.doubleSided ? 1ull : 0ull;
    const uint64_t vertexColor = instance.vertexColorOnly ? 1ull : 0ull;
    return MaterialVariantKey{
        (shader << 8)
        | (alphaMode << 5)
        | (blendMode << 3)
        | (depth << 2)
        | (sidedness << 1)
        | vertexColor
    };
}

inline MaterialVariantKey makeMaterialVariantKey(const MaterialInstance& instance)
{
    return makeMaterialVariantKey(MaterialShaderFamily::LegacyUnlit, instance);
}

inline MaterialFrameState makeMaterialFrameState(const MaterialGpuState& state)
{
    return MaterialFrameState{
        state.instance,
        state.gpuHandle,
        state.variantKey,
        renderQueueForAlphaMode(state.renderState.alphaMode),
        state.uploadedVersion,
        state.baseColorTextureID,
        state.baseColor,
        state.renderState,
        state.vertexColorOnly
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

    template<>
    struct hash<MaterialGpuHandle>
    {
        size_t operator()(MaterialGpuHandle handle) const noexcept
        {
            return (static_cast<size_t>(handle.id) << 32u) ^ static_cast<size_t>(handle.generation);
        }
    };
}
