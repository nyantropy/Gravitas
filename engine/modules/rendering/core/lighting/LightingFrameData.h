#pragma once

#include <algorithm>
#include <cmath>

#include "GlmConfig.h"

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

    inline glm::vec4 evaluateBlinnPhongLighting(const glm::vec4& baseColor,
                                                const glm::vec4& vertexColor,
                                                const glm::vec3& worldNormal,
                                                const glm::vec3& worldPosition,
                                                const glm::vec3& cameraPosition,
                                                const DirectionalLightFrameData& light,
                                                float specularStrength,
                                                float shininess)
    {
        const glm::vec4 base = baseColor * vertexColor;
        const glm::vec3 normal = safeLightingNormalize(worldNormal, {0.0f, 0.0f, 1.0f});
        const glm::vec3 directionToLight =
            safeLightingNormalize(light.directionToLight, {0.0f, 0.0f, 1.0f});
        const glm::vec3 lightColor = glm::max(light.color, glm::vec3(0.0f));
        const float lightIntensity = std::max(0.0f, light.intensity);
        const float ambient = std::max(0.0f, light.ambientIntensity);

        const glm::vec3 baseRgb = glm::vec3(base);
        const glm::vec3 ambientTerm = baseRgb * ambient;
        const float diffuseFactor = std::max(glm::dot(normal, directionToLight), 0.0f);
        const glm::vec3 diffuse =
            baseRgb * lightColor * lightIntensity * diffuseFactor;

        const glm::vec3 viewDirection =
            safeLightingNormalize(cameraPosition - worldPosition, {0.0f, 0.0f, 1.0f});
        const glm::vec3 halfDirection =
            safeLightingNormalize(directionToLight + viewDirection, directionToLight);
        const float specularFactor = std::pow(
            std::max(glm::dot(normal, halfDirection), 0.0f),
            std::max(1.0f, shininess));
        const glm::vec3 specular =
            lightColor * lightIntensity * std::max(0.0f, specularStrength) * specularFactor;

        return {ambientTerm + diffuse + specular, base.a};
    }
}
