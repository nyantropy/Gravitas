#pragma once

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

enum class MaterialTextureRole
{
    BaseColor,
    MetallicRoughness,
    Normal,
    AmbientOcclusion,
    Emissive
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
