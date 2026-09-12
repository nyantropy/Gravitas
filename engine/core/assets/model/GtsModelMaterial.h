#pragma once

#include <cstdint>
#include <limits>
#include <optional>
#include <string>

#include "GlmConfig.h"

enum class GtsModelAlphaMode
{
    Opaque,
    Mask,
    Blend
};

enum class GtsModelTextureChannel
{
    Red,
    Green,
    Blue,
    Alpha
};

struct GtsModelImageBinding
{
    static constexpr uint32_t InvalidImageIndex = std::numeric_limits<uint32_t>::max();

    uint32_t imageIndex = InvalidImageIndex;
    uint32_t texCoordSet = 0;
};

struct GtsModelScalarImageBinding
{
    GtsModelImageBinding image;
    GtsModelTextureChannel channel = GtsModelTextureChannel::Red;
};

struct GtsModelMaterial
{
    std::string name;
    glm::vec4 baseColor = glm::vec4(1.0f);
    float metallic = 0.0f;
    float roughness = 1.0f;
    glm::vec3 emissiveFactor = glm::vec3(0.0f);
    float emissiveStrength = 1.0f;
    float normalScale = 1.0f;
    float ambientOcclusionStrength = 1.0f;

    std::optional<GtsModelImageBinding> baseColorImage;
    std::optional<GtsModelScalarImageBinding> metallicImage;
    std::optional<GtsModelScalarImageBinding> roughnessImage;
    std::optional<GtsModelImageBinding> normalImage;
    std::optional<GtsModelScalarImageBinding> ambientOcclusionImage;
    std::optional<GtsModelImageBinding> emissiveImage;

    GtsModelAlphaMode alphaMode = GtsModelAlphaMode::Opaque;
    float alphaCutoff = 0.5f;
    bool doubleSided = false;
};
