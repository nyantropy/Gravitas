#pragma once

#include <algorithm>
#include <vector>

#include "DirectionalLightComponent.h"
#include "ECSWorld.hpp"
#include "LightingFrameData.h"
#include "WorldTransformComponent.h"

namespace gts::rendering
{
    inline glm::vec3 directionToLightFromWorldMatrix(const glm::mat4& worldMatrix)
    {
        // Local -Z is the direction light rays travel. Shading uses the
        // opposite vector: direction from the surface point toward the light.
        return safeLightingNormalize(glm::vec3(worldMatrix[2]), {0.0f, 0.0f, 1.0f});
    }

    inline DirectionalLightFrameData makeDirectionalLightFrameData(
        const DirectionalLightComponent& light,
        const glm::mat4& worldMatrix)
    {
        DirectionalLightFrameData frame;
        frame.directionToLight = directionToLightFromWorldMatrix(worldMatrix);
        frame.intensity = light.enabled ? std::max(0.0f, light.intensity) : 0.0f;
        frame.color = glm::max(light.color, glm::vec3(0.0f));
        frame.ambientIntensity = std::max(0.0f, light.ambientIntensity);
        frame.active = light.enabled && light.active;
        return frame;
    }

    inline DirectionalLightFrameData extractDirectionalLightFrameData(ECSWorld& world)
    {
        DirectionalLightFrameData fallback = defaultDirectionalLightFrameData();
        std::vector<Entity> candidates = world.getAllEntitiesWith<DirectionalLightComponent>();
        std::sort(candidates.begin(),
                  candidates.end(),
                  [](Entity lhs, Entity rhs)
                  {
                      return lhs.id < rhs.id;
                  });

        for (Entity entity : candidates)
        {
            const DirectionalLightComponent& light =
                world.getComponent<DirectionalLightComponent>(entity);
            if (!light.enabled || !light.active)
                continue;

            const glm::mat4 worldMatrix = world.hasComponent<WorldTransformComponent>(entity)
                ? world.getComponent<WorldTransformComponent>(entity).matrix
                : glm::mat4(1.0f);
            return makeDirectionalLightFrameData(light, worldMatrix);
        }

        return fallback;
    }
}
