#pragma once

#include <algorithm>
#include <cstdint>
#include <cmath>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "GlmConfig.h"
#include "TextureColorSpace.h"
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
    Unlit,
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

enum class MaterialTextureRole
{
    BaseColor,
    MetallicRoughness,
    Normal,
    AmbientOcclusion,
    Emissive
};

enum class MaterialFeatureFlags : uint32_t
{
    None = 0,
    HasBaseColorTexture = 1u << 0u,
    HasMetallicRoughnessTexture = 1u << 1u,
    HasNormalTexture = 1u << 2u,
    HasAmbientOcclusionTexture = 1u << 3u,
    HasEmissiveTexture = 1u << 4u
};

inline MaterialFeatureFlags operator|(MaterialFeatureFlags lhs, MaterialFeatureFlags rhs)
{
    return static_cast<MaterialFeatureFlags>(
        static_cast<uint32_t>(lhs) | static_cast<uint32_t>(rhs));
}

inline MaterialFeatureFlags& operator|=(MaterialFeatureFlags& lhs, MaterialFeatureFlags rhs)
{
    lhs = lhs | rhs;
    return lhs;
}

inline bool hasMaterialFeature(MaterialFeatureFlags flags, MaterialFeatureFlags feature)
{
    return (static_cast<uint32_t>(flags) & static_cast<uint32_t>(feature)) != 0u;
}

inline MaterialFeatureFlags withoutMaterialFeature(MaterialFeatureFlags flags,
                                                  MaterialFeatureFlags feature)
{
    return static_cast<MaterialFeatureFlags>(
        static_cast<uint32_t>(flags) & ~static_cast<uint32_t>(feature));
}

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

    // Blend selection used by backend pipeline variants.
    MaterialBlendMode blendMode = MaterialBlendMode::Alpha;
};

struct MaterialDefinition
{
    MaterialShaderFamily shaderFamily = MaterialShaderFamily::Unlit;
};

struct MaterialTextureBinding
{
    MaterialTextureSource source = MaterialTextureSource::AssetPath;
    std::string path;
    texture_id_type resolvedTextureID = 0;
    TextureColorSpace colorSpace = TextureColorSpace::SRgb;

    static MaterialTextureBinding assetPath(std::string texturePath,
                                            TextureColorSpace textureColorSpace = TextureColorSpace::SRgb)
    {
        MaterialTextureBinding binding;
        binding.source = MaterialTextureSource::AssetPath;
        binding.path = std::move(texturePath);
        binding.colorSpace = textureColorSpace;
        return binding;
    }

    static MaterialTextureBinding dataAssetPath(std::string texturePath)
    {
        return assetPath(std::move(texturePath), TextureColorSpace::Linear);
    }

    static MaterialTextureBinding resolved(texture_id_type textureID,
                                           TextureColorSpace textureColorSpace = TextureColorSpace::SRgb)
    {
        MaterialTextureBinding binding;
        binding.source = MaterialTextureSource::ResolvedTexture;
        binding.resolvedTextureID = textureID;
        binding.colorSpace = textureColorSpace;
        return binding;
    }
};

struct MaterialTextureIds
{
    texture_id_type baseColor = 0;
    texture_id_type metallicRoughness = 0;
    texture_id_type normal = 0;
    texture_id_type ambientOcclusion = 0;
    texture_id_type emissive = 0;
};

inline bool operator==(const MaterialTextureIds& lhs, const MaterialTextureIds& rhs)
{
    return lhs.baseColor == rhs.baseColor
        && lhs.metallicRoughness == rhs.metallicRoughness
        && lhs.normal == rhs.normal
        && lhs.ambientOcclusion == rhs.ambientOcclusion
        && lhs.emissive == rhs.emissive;
}

inline bool operator!=(const MaterialTextureIds& lhs, const MaterialTextureIds& rhs)
{
    return !(lhs == rhs);
}

struct MaterialTextureCacheState
{
    std::string baseColorPath;
    std::string metallicRoughnessPath;
    std::string normalPath;
    std::string ambientOcclusionPath;
    std::string emissivePath;

    TextureColorSpace baseColorColorSpace = TextureColorSpace::SRgb;
    TextureColorSpace metallicRoughnessColorSpace = TextureColorSpace::Linear;
    TextureColorSpace normalColorSpace = TextureColorSpace::Linear;
    TextureColorSpace ambientOcclusionColorSpace = TextureColorSpace::Linear;
    TextureColorSpace emissiveColorSpace = TextureColorSpace::SRgb;
};

inline bool operator==(const MaterialTextureCacheState& lhs,
                       const MaterialTextureCacheState& rhs)
{
    return lhs.baseColorPath == rhs.baseColorPath
        && lhs.metallicRoughnessPath == rhs.metallicRoughnessPath
        && lhs.normalPath == rhs.normalPath
        && lhs.ambientOcclusionPath == rhs.ambientOcclusionPath
        && lhs.emissivePath == rhs.emissivePath
        && lhs.baseColorColorSpace == rhs.baseColorColorSpace
        && lhs.metallicRoughnessColorSpace == rhs.metallicRoughnessColorSpace
        && lhs.normalColorSpace == rhs.normalColorSpace
        && lhs.ambientOcclusionColorSpace == rhs.ambientOcclusionColorSpace
        && lhs.emissiveColorSpace == rhs.emissiveColorSpace;
}

inline bool operator!=(const MaterialTextureCacheState& lhs,
                       const MaterialTextureCacheState& rhs)
{
    return !(lhs == rhs);
}

inline TextureColorSpace textureColorSpaceForRole(MaterialTextureRole role)
{
    switch (role)
    {
        case MaterialTextureRole::BaseColor:
        case MaterialTextureRole::Emissive:
            return TextureColorSpace::SRgb;
        case MaterialTextureRole::MetallicRoughness:
        case MaterialTextureRole::Normal:
        case MaterialTextureRole::AmbientOcclusion:
            return TextureColorSpace::Linear;
    }
    return TextureColorSpace::SRgb;
}

inline const char* fallbackTextureNameForRole(MaterialTextureRole role)
{
    switch (role)
    {
        case MaterialTextureRole::BaseColor:
            return "engine_pbr_base_white.png";
        case MaterialTextureRole::MetallicRoughness:
            return "engine_pbr_metallic_roughness_neutral.png";
        case MaterialTextureRole::Normal:
            return "engine_pbr_normal_flat.png";
        case MaterialTextureRole::AmbientOcclusion:
            return "engine_pbr_ao_white.png";
        case MaterialTextureRole::Emissive:
            return "engine_pbr_emissive_black.png";
    }
    return "engine_pbr_base_white.png";
}

inline bool materialTextureBindingPresent(const MaterialTextureBinding& binding)
{
    if (binding.source == MaterialTextureSource::ResolvedTexture)
        return binding.resolvedTextureID != 0;

    if (binding.source == MaterialTextureSource::AssetPath)
        return !binding.path.empty();

    return false;
}

inline constexpr float MaterialMinimumRoughness = 0.04f;

inline float sanitizeMaterialMetallic(float metallic)
{
    if (!std::isfinite(metallic))
        return 0.0f;
    return std::clamp(metallic, 0.0f, 1.0f);
}

inline float sanitizeMaterialRoughness(float roughness)
{
    if (!std::isfinite(roughness))
        return 1.0f;
    return std::clamp(roughness, MaterialMinimumRoughness, 1.0f);
}

inline float sanitizeMaterialNormalScale(float normalScale)
{
    if (!std::isfinite(normalScale))
        return 1.0f;
    return normalScale;
}

inline float sanitizeMaterialAmbientOcclusionStrength(float strength)
{
    if (!std::isfinite(strength))
        return 1.0f;
    return std::clamp(strength, 0.0f, 1.0f);
}

inline glm::vec3 sanitizeMaterialEmissiveFactor(const glm::vec3& emissive)
{
    return {
        std::isfinite(emissive.x) ? std::max(0.0f, emissive.x) : 0.0f,
        std::isfinite(emissive.y) ? std::max(0.0f, emissive.y) : 0.0f,
        std::isfinite(emissive.z) ? std::max(0.0f, emissive.z) : 0.0f
    };
}

inline float sanitizeMaterialEmissiveStrength(float strength)
{
    if (!std::isfinite(strength))
        return 1.0f;
    return std::max(0.0f, strength);
}

struct MaterialGpuParameters
{
    // StandardSurface base-color factor. The final base color multiplies this
    // by the base-color texture and vertex color.
    glm::vec4 baseColor = {1.0f, 1.0f, 1.0f, 1.0f};
    // x = metallic, y = perceptual roughness, z = normal scale,
    // w = ambient occlusion strength.
    glm::vec4 surfaceParameters = {0.0f, 1.0f, 1.0f, 1.0f};
    // xyz = emissive factor, w = emissive strength.
    glm::vec4 emissiveFactorStrength = {0.0f, 0.0f, 0.0f, 1.0f};

    float metallic() const
    {
        return surfaceParameters.x;
    }

    float roughness() const
    {
        return surfaceParameters.y;
    }

    float normalScale() const
    {
        return surfaceParameters.z;
    }

    float ambientOcclusionStrength() const
    {
        return surfaceParameters.w;
    }

    glm::vec3 emissiveFactor() const
    {
        return glm::vec3(emissiveFactorStrength);
    }

    float emissiveStrength() const
    {
        return emissiveFactorStrength.w;
    }
};

inline MaterialGpuParameters makeMaterialGpuParameters(const glm::vec4& baseColor,
                                                       float metallic,
                                                       float roughness,
                                                       float normalScale,
                                                       float ambientOcclusionStrength,
                                                       const glm::vec3& emissiveFactor,
                                                       float emissiveStrength)
{
    MaterialGpuParameters parameters;
    parameters.baseColor = baseColor;
    parameters.surfaceParameters = {
        sanitizeMaterialMetallic(metallic),
        sanitizeMaterialRoughness(roughness),
        sanitizeMaterialNormalScale(normalScale),
        sanitizeMaterialAmbientOcclusionStrength(ambientOcclusionStrength)
    };
    parameters.emissiveFactorStrength = glm::vec4(
        sanitizeMaterialEmissiveFactor(emissiveFactor),
        sanitizeMaterialEmissiveStrength(emissiveStrength));
    return parameters;
}

inline MaterialGpuParameters makeMaterialGpuParameters(const glm::vec4& baseColor,
                                                       float metallic,
                                                       float roughness)
{
    return makeMaterialGpuParameters(
        baseColor,
        metallic,
        roughness,
        1.0f,
        1.0f,
        {0.0f, 0.0f, 0.0f},
        1.0f);
}

struct MaterialInstance
{
    MaterialDefinitionHandle definition;
    glm::vec4 baseColor = {1.0f, 1.0f, 1.0f, 1.0f};
    MaterialTextureBinding baseColorTexture = MaterialTextureBinding::assetPath({});
    MaterialTextureBinding metallicRoughnessTexture =
        MaterialTextureBinding::dataAssetPath({});
    MaterialTextureBinding normalTexture = MaterialTextureBinding::dataAssetPath({});
    MaterialTextureBinding ambientOcclusionTexture =
        MaterialTextureBinding::dataAssetPath({});
    MaterialTextureBinding emissiveTexture =
        MaterialTextureBinding::assetPath({});
    float metallic = 0.0f;
    float roughness = 1.0f;
    float normalScale = 1.0f;
    float ambientOcclusionStrength = 1.0f;
    glm::vec3 emissiveFactor = {0.0f, 0.0f, 0.0f};
    float emissiveStrength = 1.0f;
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
    MaterialShaderFamily shaderFamily = MaterialShaderFamily::Unlit;
    MaterialTextureIds textures{};
    texture_id_type baseColorTextureID = 0;
    MaterialTextureCacheState textureCache{};
    MaterialFeatureFlags featureFlags = MaterialFeatureFlags::None;
    MaterialGpuParameters parameters{};
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
    MaterialShaderFamily shaderFamily = MaterialShaderFamily::Unlit;

    // Backend frame data owned by material state; render commands and object
    // uploads must not duplicate it.
    MaterialTextureIds textures{};
    texture_id_type baseColorTextureID = 0;
    MaterialFeatureFlags featureFlags = MaterialFeatureFlags::None;
    MaterialGpuParameters parameters{};
    MaterialRenderState renderState{};
    bool vertexColorOnly = false;
};

struct MaterialGpuHandleHash
{
    size_t operator()(MaterialGpuHandle handle) const noexcept
    {
        return (static_cast<size_t>(handle.id) << 32u) ^ static_cast<size_t>(handle.generation);
    }
};

struct MaterialFrameData
{
    std::vector<MaterialFrameState> materials;
    std::unordered_map<MaterialGpuHandle, size_t, MaterialGpuHandleHash> materialIndexByGpuHandle;

    void clear()
    {
        materials.clear();
        materialIndexByGpuHandle.clear();
    }

    void reserve(size_t count)
    {
        materials.reserve(count);
        materialIndexByGpuHandle.reserve(count);
    }

    const MaterialFrameState* find(MaterialGpuHandle handle) const
    {
        const auto it = materialIndexByGpuHandle.find(handle);
        if (it == materialIndexByGpuHandle.end() || it->second >= materials.size())
            return nullptr;

        const MaterialFrameState& material = materials[it->second];
        return material.gpuHandle == handle ? &material : nullptr;
    }

    void upsert(const MaterialFrameState& material)
    {
        const auto it = materialIndexByGpuHandle.find(material.gpuHandle);
        if (it != materialIndexByGpuHandle.end() && it->second < materials.size())
        {
            materials[it->second] = material;
            return;
        }

        materialIndexByGpuHandle[material.gpuHandle] = materials.size();
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
                                                 const MaterialInstance& instance,
                                                 MaterialFeatureFlags featureFlags)
{
    const uint64_t shader = static_cast<uint64_t>(shaderFamily) & 0xFFull;
    const uint64_t alphaMode = static_cast<uint64_t>(instance.renderState.alphaMode) & 0x3ull;
    // Blend mode is part of the backend material variant key while alpha and
    // additive rendering use separate pipeline states.
    const uint64_t blendMode = static_cast<uint64_t>(instance.renderState.blendMode) & 0x3ull;
    const uint64_t depth = instance.renderState.depthWrite ? 1ull : 0ull;
    const uint64_t sidedness = instance.renderState.doubleSided ? 1ull : 0ull;
    const uint64_t vertexColor = instance.vertexColorOnly ? 1ull : 0ull;
    const uint64_t normalMap =
        hasMaterialFeature(featureFlags, MaterialFeatureFlags::HasNormalTexture) ? 1ull : 0ull;
    return MaterialVariantKey{
        (normalMap << 16)
        | (shader << 8)
        | (alphaMode << 5)
        | (blendMode << 3)
        | (depth << 2)
        | (sidedness << 1)
        | vertexColor
    };
}

inline MaterialFeatureFlags materialFeatureFlagsForInstance(const MaterialInstance& instance)
{
    MaterialFeatureFlags flags = MaterialFeatureFlags::None;
    if (materialTextureBindingPresent(instance.baseColorTexture))
        flags |= MaterialFeatureFlags::HasBaseColorTexture;
    if (materialTextureBindingPresent(instance.metallicRoughnessTexture))
        flags |= MaterialFeatureFlags::HasMetallicRoughnessTexture;
    if (materialTextureBindingPresent(instance.normalTexture))
        flags |= MaterialFeatureFlags::HasNormalTexture;
    if (materialTextureBindingPresent(instance.ambientOcclusionTexture))
        flags |= MaterialFeatureFlags::HasAmbientOcclusionTexture;
    if (materialTextureBindingPresent(instance.emissiveTexture))
        flags |= MaterialFeatureFlags::HasEmissiveTexture;
    return flags;
}

inline MaterialVariantKey makeMaterialVariantKey(MaterialShaderFamily shaderFamily,
                                                 const MaterialInstance& instance)
{
    return makeMaterialVariantKey(
        shaderFamily,
        instance,
        materialFeatureFlagsForInstance(instance));
}

inline MaterialVariantKey makeMaterialVariantKey(const MaterialInstance& instance)
{
    return makeMaterialVariantKey(MaterialShaderFamily::Unlit, instance);
}

inline MaterialFrameState makeMaterialFrameState(const MaterialGpuState& state)
{
    return MaterialFrameState{
        state.instance,
        state.gpuHandle,
        state.variantKey,
        renderQueueForAlphaMode(state.renderState.alphaMode),
        state.uploadedVersion,
        state.shaderFamily,
        state.textures,
        state.baseColorTextureID,
        state.featureFlags,
        state.parameters,
        state.renderState,
        state.vertexColorOnly
    };
}

inline MaterialAlphaMode alphaModeForBlendMode(MaterialBlendMode blendMode,
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
