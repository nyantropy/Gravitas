#pragma once

#include <algorithm>
#include <cmath>

#include "GlmConfig.h"
#include "MaterialTypes.h"

namespace gts::rendering
{
    struct DirectionalLightFrameData
    {
        // Direction from a shaded point toward the directional light.
        glm::vec3 directionToLight = {0.0f, 0.0f, 1.0f};
        float intensity = 0.0f;

        glm::vec3 color = {1.0f, 1.0f, 1.0f};
        float ambientIntensity = 0.12f;

        bool active = false;
    };

    inline DirectionalLightFrameData defaultDirectionalLightFrameData()
    {
        return DirectionalLightFrameData{};
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
                                                             float metallic,
                                                             float roughness)
    {
        const glm::vec4 base = baseColor * vertexColor;
        const glm::vec3 normal = safeLightingNormalize(worldNormal, {0.0f, 0.0f, 1.0f});
        const glm::vec3 directionToLight =
            safeLightingNormalize(light.directionToLight, {0.0f, 0.0f, 1.0f});
        const glm::vec3 lightColor = glm::max(light.color, glm::vec3(0.0f));
        const float lightIntensity = std::max(0.0f, light.intensity);
        const float ambient = std::max(0.0f, light.ambientIntensity);
        const float clampedMetallic = sanitizeMaterialMetallic(metallic);
        const float clampedRoughness = sanitizeMaterialRoughness(roughness);

        const glm::vec3 baseRgb = glm::vec3(base);
        const glm::vec3 viewDirection =
            safeLightingNormalize(cameraPosition - worldPosition, {0.0f, 0.0f, 1.0f});
        const glm::vec3 halfDirection =
            safeLightingNormalize(directionToLight + viewDirection, directionToLight);

        const float nDotL = saturateLighting(glm::dot(normal, directionToLight));
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
        const glm::vec3 radiance = lightColor * lightIntensity;
        const glm::vec3 direct = (nDotL > 0.0f && nDotV > 0.0f)
            ? (diffuse + specular) * radiance * nDotL
            : glm::vec3(0.0f);
        const glm::vec3 ambientTerm = baseRgb * (1.0f - clampedMetallic) * ambient;
        const glm::vec3 result = glm::max(ambientTerm + direct, glm::vec3(0.0f));

        return {result, base.a};
    }
}
