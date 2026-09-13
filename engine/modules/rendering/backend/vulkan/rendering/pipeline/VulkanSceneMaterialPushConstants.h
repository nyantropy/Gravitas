#pragma once
#include "GlmConfig.h"
struct VulkanSceneMaterialPushConstants
    {
        // x = vertex-color-only, y = material feature flags.
        glm::ivec4 materialFlags = {0, 0, 0, 0};
        // Shared material base-color factor.
        glm::vec4 baseColor = {1.0f, 1.0f, 1.0f, 1.0f};
        // x = metallic, y = roughness, z = normalScale, w = AO strength.
        glm::vec4 surfaceFactors = {0.0f, 1.0f, 1.0f, 1.0f};
        // xyz = emissive factor, w = emissive strength.
        glm::vec4 emissiveFactorStrength = {0.0f, 0.0f, 0.0f, 1.0f};
    };

