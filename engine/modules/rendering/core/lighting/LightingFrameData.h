#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string>

#include "GlmConfig.h"
#include "MaterialTypes.h"

namespace gts::rendering
{
    inline constexpr uint32_t MaxDirectionalLights = 2;
    inline constexpr uint32_t MaxPointLights = 32;
    inline constexpr uint32_t MaxSpotLights = 16;
    inline constexpr float MinLightRange = 0.01f;
    inline constexpr float MinConeSeparationRadians = 0.001f;
    inline constexpr float MaxEnvironmentPrefilterMip = 5.0f;
    inline constexpr uint32_t EnvironmentCubemapFaceCount = 6;
    inline constexpr uint32_t EnvironmentPrefilterMipCount = 6;

    enum class EnvironmentCubemapFace : uint32_t
    {
        PositiveX = 0,
        NegativeX = 1,
        PositiveY = 2,
        NegativeY = 3,
        PositiveZ = 4,
        NegativeZ = 5
    };

    struct DirectionalLightFrameData
    {
        // Direction from a shaded point toward the directional light.
        glm::vec3 directionToLight = {0.0f, 0.0f, 1.0f};
        float intensity = 0.0f;

        glm::vec3 color = {1.0f, 1.0f, 1.0f};
        float padding = 0.0f;

        bool active = false;
    };

    struct PointLightFrameData
    {
        glm::vec3 position = {0.0f, 0.0f, 0.0f};
        float range = 1.0f;

        glm::vec3 color = {1.0f, 1.0f, 1.0f};
        float intensity = 0.0f;
    };

    struct SpotLightFrameData
    {
        glm::vec3 position = {0.0f, 0.0f, 0.0f};
        float range = 1.0f;

        // Direction light rays travel from the spot origin.
        glm::vec3 directionFromLight = {0.0f, 0.0f, -1.0f};
        float intensity = 0.0f;

        glm::vec3 color = {1.0f, 1.0f, 1.0f};
        float innerConeCos = 0.9063078f;

        float outerConeCos = 0.8191520f;
        glm::vec3 padding = {0.0f, 0.0f, 0.0f};
    };

    struct LightingDiagnostics
    {
        uint32_t totalDirectionalLights = 0;
        uint32_t totalPointLights = 0;
        uint32_t totalSpotLights = 0;
        uint32_t totalEnvironmentLights = 0;
        uint32_t droppedDirectionalLights = 0;
        uint32_t droppedPointLights = 0;
        uint32_t droppedSpotLights = 0;
        uint32_t droppedEnvironmentLights = 0;
        uint32_t sanitizedLights = 0;
        uint32_t sanitizedEnvironmentLights = 0;
    };

    struct EnvironmentFrameData
    {
        // Empty path means the backend should use built-in neutral fallback resources.
        std::string environmentPath;
        float intensity = 0.0f;
        float rotationRadians = 0.0f;
        bool enabled = false;
    };

    struct LightingFrameData
    {
        std::array<DirectionalLightFrameData, MaxDirectionalLights> directional{};
        std::array<PointLightFrameData, MaxPointLights> point{};
        std::array<SpotLightFrameData, MaxSpotLights> spot{};

        uint32_t directionalCount = 0;
        uint32_t pointCount = 0;
        uint32_t spotCount = 0;
        float ambientIntensity = 0.12f;

        EnvironmentFrameData environment{};
        LightingDiagnostics diagnostics{};
    };

    inline DirectionalLightFrameData defaultDirectionalLightFrameData()
    {
        return DirectionalLightFrameData{};
    }

    inline LightingFrameData defaultLightingFrameData()
    {
        return LightingFrameData{};
    }

    inline glm::vec3 safeLightingNormalize(const glm::vec3& value,
                                           const glm::vec3& fallback)
    {
        const float lengthSquared = glm::dot(value, value);
        if (lengthSquared <= 1.0e-12f || !std::isfinite(lengthSquared))
            return fallback;

        const glm::vec3 normalized = value * (1.0f / std::sqrt(lengthSquared));
        if (!std::isfinite(normalized.x) || !std::isfinite(normalized.y) ||
            !std::isfinite(normalized.z))
        {
            return fallback;
        }

        return normalized;
    }

    inline glm::vec3 transformNormalToWorld(const glm::mat4& modelMatrix,
                                            const glm::vec3& localNormal)
    {
        const glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(modelMatrix)));
        return safeLightingNormalize(normalMatrix * localNormal, {0.0f, 0.0f, 1.0f});
    }

    inline constexpr float PbrPi = 3.14159265358979323846f;
    inline constexpr float PbrEpsilon = 1.0e-5f;

    inline float saturateLighting(float value)
    {
        if (!std::isfinite(value))
            return 0.0f;
        return std::clamp(value, 0.0f, 1.0f);
    }

    inline glm::vec3 pbrDielectricF0()
    {
        return {0.04f, 0.04f, 0.04f};
    }

    inline glm::vec3 pbrF0ForBaseColor(const glm::vec3& baseColor, float metallic);
    inline glm::vec3 fresnelSchlick(float cosTheta, const glm::vec3& f0);
    inline glm::vec3 fresnelSchlickRoughness(float cosTheta,
                                             const glm::vec3& f0,
                                             float roughness);
    inline float distributionGGX(float nDotH, float roughness);
    inline float geometrySmith(float nDotV, float nDotL, float roughness);

    inline float sanitizeLightIntensity(float intensity)
    {
        if (!std::isfinite(intensity))
            return 0.0f;
        return std::max(0.0f, intensity);
    }

    inline float sanitizeLightRange(float range)
    {
        if (!std::isfinite(range))
            return 1.0f;
        return std::max(MinLightRange, range);
    }

    inline glm::vec3 sanitizeLightColor(const glm::vec3& color)
    {
        return {
            std::isfinite(color.x) ? std::max(0.0f, color.x) : 0.0f,
            std::isfinite(color.y) ? std::max(0.0f, color.y) : 0.0f,
            std::isfinite(color.z) ? std::max(0.0f, color.z) : 0.0f
        };
    }

    inline void sanitizeSpotConeAngles(float authoredInner,
                                       float authoredOuter,
                                       float& sanitizedInner,
                                       float& sanitizedOuter)
    {
        constexpr float MaxCone = PbrPi - 0.001f;
        sanitizedInner = std::isfinite(authoredInner)
            ? std::clamp(authoredInner, 0.0f, MaxCone)
            : 0.43633232f;
        sanitizedOuter = std::isfinite(authoredOuter)
            ? std::clamp(authoredOuter, 0.0f, MaxCone)
            : 0.61086524f;
        if (sanitizedOuter < sanitizedInner + MinConeSeparationRadians)
            sanitizedOuter = std::min(MaxCone, sanitizedInner + MinConeSeparationRadians);
    }

    inline float pointLightAttenuation(float distance, float range)
    {
        const float clampedRange = sanitizeLightRange(range);
        if (!std::isfinite(distance) || distance >= clampedRange)
            return 0.0f;

        const float clampedDistance = std::max(0.0f, distance);
        const float distanceSquared =
            std::max(clampedDistance * clampedDistance, 0.01f);
        const float normalizedDistance = saturateLighting(clampedDistance / clampedRange);
        const float falloffBase = 1.0f - normalizedDistance * normalizedDistance *
            normalizedDistance * normalizedDistance;
        const float cutoff = saturateLighting(falloffBase) * saturateLighting(falloffBase);
        return cutoff / distanceSquared;
    }

    inline float spotConeAttenuation(float coneCos, float innerConeCos, float outerConeCos)
    {
        if (!std::isfinite(coneCos) || !std::isfinite(innerConeCos) || !std::isfinite(outerConeCos))
            return 0.0f;

        if (innerConeCos <= outerConeCos + 1.0e-5f)
            return coneCos >= innerConeCos ? 1.0f : 0.0f;

        const float t = saturateLighting((coneCos - outerConeCos) / (innerConeCos - outerConeCos));
        return t * t * (3.0f - 2.0f * t);
    }

    inline glm::vec4 combineBaseColor(const glm::vec4& baseColorFactor,
                                      const glm::vec4& sampledBaseColor,
                                      const glm::vec4& vertexColor)
    {
        return baseColorFactor * sampledBaseColor * vertexColor;
    }

    inline glm::vec2 decodeMetallicRoughness(const glm::vec4& sampledMetallicRoughness,
                                             float metallicFactor,
                                             float roughnessFactor)
    {
        return {
            sanitizeMaterialMetallic(metallicFactor * sampledMetallicRoughness.b),
            sanitizeMaterialRoughness(roughnessFactor * sampledMetallicRoughness.g)
        };
    }

    inline glm::vec3 decodeTangentSpaceNormal(const glm::vec3& encodedNormal,
                                              float normalScale)
    {
        glm::vec3 tangentNormal = encodedNormal * 2.0f - glm::vec3(1.0f);
        const float scale = sanitizeMaterialNormalScale(normalScale);
        tangentNormal.x *= scale;
        tangentNormal.y *= scale;
        return safeLightingNormalize(tangentNormal, {0.0f, 0.0f, 1.0f});
    }

    inline glm::vec3 fallbackLightingTangentForNormal(const glm::vec3& normal)
    {
        const glm::vec3 normalized = safeLightingNormalize(normal, {0.0f, 0.0f, 1.0f});
        if (std::abs(normalized.z) < 0.999f)
        {
            return safeLightingNormalize(
                glm::cross(glm::vec3(0.0f, 0.0f, 1.0f), normalized),
                {1.0f, 0.0f, 0.0f});
        }
        return {1.0f, 0.0f, 0.0f};
    }

    inline glm::vec3 applyTangentSpaceNormalMap(const glm::vec3& worldNormal,
                                                const glm::vec4& worldTangent,
                                                const glm::vec3& encodedNormal,
                                                float normalScale)
    {
        const glm::vec3 normal = safeLightingNormalize(worldNormal, {0.0f, 0.0f, 1.0f});
        glm::vec3 tangent = safeLightingNormalize(
            glm::vec3(worldTangent),
            fallbackLightingTangentForNormal(normal));
        tangent = tangent - normal * glm::dot(tangent, normal);
        tangent = safeLightingNormalize(tangent, fallbackLightingTangentForNormal(normal));
        const glm::vec3 bitangent = safeLightingNormalize(
            glm::cross(normal, tangent) * worldTangent.w,
            {0.0f, 1.0f, 0.0f});
        const glm::vec3 mappedNormal = decodeTangentSpaceNormal(encodedNormal, normalScale);
        return safeLightingNormalize(
            glm::mat3(tangent, bitangent, normal) * mappedNormal,
            normal);
    }

    inline glm::vec3 applyAmbientOcclusion(const glm::vec3& ambient,
                                           float sampledAmbientOcclusion,
                                           float strength)
    {
        const float occlusion = std::isfinite(sampledAmbientOcclusion)
            ? saturateLighting(sampledAmbientOcclusion)
            : 1.0f;
        return ambient * glm::mix(
            1.0f,
            occlusion,
            sanitizeMaterialAmbientOcclusionStrength(strength));
    }

    inline glm::vec3 evaluateEmissive(const glm::vec3& emissiveFactor,
                                      float emissiveStrength,
                                      const glm::vec3& sampledEmissive)
    {
        return sanitizeMaterialEmissiveFactor(emissiveFactor)
            * sanitizeMaterialEmissiveStrength(emissiveStrength)
            * glm::max(sampledEmissive, glm::vec3(0.0f));
    }

    inline float sanitizeEnvironmentIntensity(float intensity)
    {
        if (!std::isfinite(intensity))
            return 0.0f;
        return std::max(0.0f, intensity);
    }

    inline float sanitizeEnvironmentRotation(float rotationRadians)
    {
        return std::isfinite(rotationRadians) ? rotationRadians : 0.0f;
    }

    inline glm::vec3 rotateEnvironmentDirectionY(const glm::vec3& direction,
                                                 float rotationRadians)
    {
        const glm::vec3 normalized = safeLightingNormalize(direction, {0.0f, 0.0f, 1.0f});
        const float rotation = sanitizeEnvironmentRotation(rotationRadians);
        const float c = std::cos(rotation);
        const float s = std::sin(rotation);
        return safeLightingNormalize(
            {
                normalized.x * c - normalized.z * s,
                normalized.y,
                normalized.x * s + normalized.z * c
            },
            normalized);
    }

    inline glm::vec2 equirectangularUvForDirection(const glm::vec3& direction)
    {
        const glm::vec3 d = safeLightingNormalize(direction, {0.0f, 0.0f, 1.0f});
        const float u = std::atan2(d.z, d.x) / (2.0f * PbrPi) + 0.5f;
        const float v = std::acos(std::clamp(d.y, -1.0f, 1.0f)) / PbrPi;
        return {u, v};
    }

    inline glm::vec3 cubemapDirectionForFaceUv(EnvironmentCubemapFace face,
                                               float u,
                                               float v)
    {
        const float x = 2.0f * saturateLighting(u) - 1.0f;
        const float y = 2.0f * saturateLighting(v) - 1.0f;
        switch (face)
        {
            case EnvironmentCubemapFace::PositiveX:
                return safeLightingNormalize({1.0f, -y, -x}, {1.0f, 0.0f, 0.0f});
            case EnvironmentCubemapFace::NegativeX:
                return safeLightingNormalize({-1.0f, -y, x}, {-1.0f, 0.0f, 0.0f});
            case EnvironmentCubemapFace::PositiveY:
                return safeLightingNormalize({x, 1.0f, y}, {0.0f, 1.0f, 0.0f});
            case EnvironmentCubemapFace::NegativeY:
                return safeLightingNormalize({x, -1.0f, -y}, {0.0f, -1.0f, 0.0f});
            case EnvironmentCubemapFace::PositiveZ:
                return safeLightingNormalize({x, -y, 1.0f}, {0.0f, 0.0f, 1.0f});
            case EnvironmentCubemapFace::NegativeZ:
                return safeLightingNormalize({-x, -y, -1.0f}, {0.0f, 0.0f, -1.0f});
        }
        return {0.0f, 0.0f, 1.0f};
    }

    inline float roughnessToEnvironmentMip(float roughness, float maxMipLevel)
    {
        const float clampedRoughness = sanitizeMaterialRoughness(roughness);
        const float clampedMaxMip =
            std::isfinite(maxMipLevel) ? std::max(0.0f, maxMipLevel) : 0.0f;
        return clampedRoughness * clampedMaxMip;
    }

    inline glm::vec3 evaluateDiffuseIbl(const glm::vec3& baseRgb,
                                        const glm::vec3& fresnel,
                                        float metallic,
                                        const glm::vec3& irradiance,
                                        float ambientOcclusion)
    {
        const glm::vec3 kd =
            (glm::vec3(1.0f) - glm::clamp(fresnel, glm::vec3(0.0f), glm::vec3(1.0f))) *
            (1.0f - sanitizeMaterialMetallic(metallic));
        return kd *
            glm::max(baseRgb, glm::vec3(0.0f)) *
            glm::max(irradiance, glm::vec3(0.0f)) *
            saturateLighting(ambientOcclusion);
    }

    inline glm::vec3 evaluateSpecularIbl(const glm::vec3& f0,
                                         float nDotV,
                                         float roughness,
                                         const glm::vec3& prefilteredRadiance,
                                         const glm::vec2& brdfSample)
    {
        const glm::vec3 fresnel = fresnelSchlickRoughness(nDotV, f0, roughness);
        const glm::vec2 brdf = glm::max(brdfSample, glm::vec2(0.0f));
        return glm::max(prefilteredRadiance, glm::vec3(0.0f)) *
            (fresnel * brdf.x + brdf.y);
    }

    inline glm::vec3 evaluatePbrDirectContribution(const glm::vec3& baseRgb,
                                                   const glm::vec3& normal,
                                                   const glm::vec3& viewDirection,
                                                   const glm::vec3& directionToLight,
                                                   const glm::vec3& radiance,
                                                   float metallic,
                                                   float roughness)
    {
        const float clampedMetallic = sanitizeMaterialMetallic(metallic);
        const float clampedRoughness = sanitizeMaterialRoughness(roughness);
        const glm::vec3 lightDirection =
            safeLightingNormalize(directionToLight, {0.0f, 0.0f, 1.0f});
        const glm::vec3 halfDirection =
            safeLightingNormalize(lightDirection + viewDirection, lightDirection);

        const float nDotL = saturateLighting(glm::dot(normal, lightDirection));
        const float nDotV = saturateLighting(glm::dot(normal, viewDirection));
        const float nDotH = saturateLighting(glm::dot(normal, halfDirection));
        const float hDotV = saturateLighting(glm::dot(halfDirection, viewDirection));

        const glm::vec3 f0 = pbrF0ForBaseColor(baseRgb, clampedMetallic);
        const glm::vec3 fresnel = fresnelSchlick(hDotV, f0);
        const float distribution = distributionGGX(nDotH, clampedRoughness);
        const float geometry = geometrySmith(nDotV, nDotL, clampedRoughness);
        const glm::vec3 specular =
            (distribution * geometry * fresnel) /
            std::max(4.0f * nDotV * nDotL, PbrEpsilon);

        const glm::vec3 kd = (glm::vec3(1.0f) - fresnel) * (1.0f - clampedMetallic);
        const glm::vec3 diffuse = kd * baseRgb / PbrPi;
        return (nDotL > 0.0f && nDotV > 0.0f)
            ? (diffuse + specular) * glm::max(radiance, glm::vec3(0.0f)) * nDotL
            : glm::vec3(0.0f);
    }

    inline glm::vec3 pbrF0ForBaseColor(const glm::vec3& baseColor, float metallic)
    {
        const float sanitizedMetallic = sanitizeMaterialMetallic(metallic);
        return glm::mix(pbrDielectricF0(), glm::max(baseColor, glm::vec3(0.0f)), sanitizedMetallic);
    }

    inline glm::vec3 fresnelSchlick(float cosTheta, const glm::vec3& f0)
    {
        const float clampedCosTheta = saturateLighting(cosTheta);
        return f0 + (glm::vec3(1.0f) - f0) *
            std::pow(1.0f - clampedCosTheta, 5.0f);
    }

    inline glm::vec3 fresnelSchlickRoughness(float cosTheta,
                                             const glm::vec3& f0,
                                             float roughness)
    {
        const float clampedCosTheta = saturateLighting(cosTheta);
        const float clampedRoughness = sanitizeMaterialRoughness(roughness);
        const glm::vec3 upper = glm::max(glm::vec3(1.0f - clampedRoughness), f0);
        return f0 + (upper - f0) * std::pow(1.0f - clampedCosTheta, 5.0f);
    }

    inline float distributionGGX(float nDotH, float roughness)
    {
        const float clampedNDotH = saturateLighting(nDotH);
        const float clampedRoughness = sanitizeMaterialRoughness(roughness);
        const float alpha = clampedRoughness * clampedRoughness;
        const float alphaSquared = alpha * alpha;
        const float denominator =
            (clampedNDotH * clampedNDotH) * (alphaSquared - 1.0f) + 1.0f;
        return alphaSquared /
            std::max(PbrPi * denominator * denominator, PbrEpsilon);
    }

    inline float geometrySchlickGGX(float nDot, float roughness)
    {
        const float clampedNDot = saturateLighting(nDot);
        const float clampedRoughness = sanitizeMaterialRoughness(roughness);
        const float r = clampedRoughness + 1.0f;
        const float k = (r * r) / 8.0f;
        return clampedNDot / std::max(clampedNDot * (1.0f - k) + k, PbrEpsilon);
    }

    inline float geometrySmith(float nDotV, float nDotL, float roughness)
    {
        return geometrySchlickGGX(nDotV, roughness) *
            geometrySchlickGGX(nDotL, roughness);
    }

    inline glm::vec4 evaluateMetallicRoughnessDirectLighting(const glm::vec4& baseColor,
                                                             const glm::vec4& vertexColor,
                                                             const glm::vec3& worldNormal,
                                                             const glm::vec3& worldPosition,
                                                             const glm::vec3& cameraPosition,
                                                             const DirectionalLightFrameData& light,
                                                             float ambientIntensity,
                                                             float metallic,
                                                             float roughness)
    {
        const glm::vec4 base = baseColor * vertexColor;
        const glm::vec3 normal = safeLightingNormalize(worldNormal, {0.0f, 0.0f, 1.0f});
        const glm::vec3 directionToLight =
            safeLightingNormalize(light.directionToLight, {0.0f, 0.0f, 1.0f});
        const glm::vec3 lightColor = glm::max(light.color, glm::vec3(0.0f));
        const float lightIntensity = std::max(0.0f, light.intensity);
        const float ambient = std::max(0.0f, ambientIntensity);
        const float clampedMetallic = sanitizeMaterialMetallic(metallic);
        const float clampedRoughness = sanitizeMaterialRoughness(roughness);

        const glm::vec3 baseRgb = glm::vec3(base);
        const glm::vec3 viewDirection =
            safeLightingNormalize(cameraPosition - worldPosition, {0.0f, 0.0f, 1.0f});
        const glm::vec3 radiance = lightColor * lightIntensity;
        const glm::vec3 direct = evaluatePbrDirectContribution(
            baseRgb,
            normal,
            viewDirection,
            directionToLight,
            radiance,
            clampedMetallic,
            clampedRoughness);
        const glm::vec3 ambientTerm = baseRgb * (1.0f - clampedMetallic) * ambient;
        const glm::vec3 result = glm::max(ambientTerm + direct, glm::vec3(0.0f));

        return {result, base.a};
    }
}
