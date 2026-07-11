#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

#include "DirectionalLightComponent.h"
#include "ECSWorld.hpp"
#include "LightingFrameData.h"
#include "PointLightComponent.h"
#include "SpotLightComponent.h"
#include "WorldTransformComponent.h"

namespace gts::rendering
{
    inline glm::vec3 positionFromWorldMatrix(const glm::mat4& worldMatrix)
    {
        return glm::vec3(worldMatrix[3]);
    }

    inline glm::vec3 directionToLightFromWorldMatrix(const glm::mat4& worldMatrix)
    {
        // Local -Z is the direction light rays travel. Shading uses the
        // opposite vector: direction from the surface point toward the light.
        return safeLightingNormalize(glm::vec3(worldMatrix[2]), {0.0f, 0.0f, 1.0f});
    }

    inline glm::vec3 directionFromLightFromWorldMatrix(const glm::mat4& worldMatrix)
    {
        return -directionToLightFromWorldMatrix(worldMatrix);
    }

    inline glm::mat4 resolvedLightWorldMatrix(ECSWorld& world, Entity entity)
    {
        return world.hasComponent<WorldTransformComponent>(entity)
            ? world.getComponent<WorldTransformComponent>(entity).matrix
            : glm::mat4(1.0f);
    }

    inline DirectionalLightFrameData makeDirectionalLightFrameData(
        const DirectionalLightComponent& light,
        const glm::mat4& worldMatrix)
    {
        DirectionalLightFrameData frame;
        frame.directionToLight = directionToLightFromWorldMatrix(worldMatrix);
        frame.intensity = sanitizeLightIntensity(light.intensity);
        frame.color = sanitizeLightColor(light.color);
        frame.active = light.enabled && light.active;
        return frame;
    }

    inline PointLightFrameData makePointLightFrameData(const PointLightComponent& light,
                                                       const glm::mat4& worldMatrix)
    {
        PointLightFrameData frame;
        frame.position = positionFromWorldMatrix(worldMatrix);
        frame.range = sanitizeLightRange(light.range);
        frame.color = sanitizeLightColor(light.color);
        frame.intensity = sanitizeLightIntensity(light.intensity);
        return frame;
    }

    inline SpotLightFrameData makeSpotLightFrameData(const SpotLightComponent& light,
                                                     const glm::mat4& worldMatrix)
    {
        float innerAngle = 0.0f;
        float outerAngle = 0.0f;
        sanitizeSpotConeAngles(light.innerConeAngle, light.outerConeAngle, innerAngle, outerAngle);

        SpotLightFrameData frame;
        frame.position = positionFromWorldMatrix(worldMatrix);
        frame.range = sanitizeLightRange(light.range);
        frame.directionFromLight = directionFromLightFromWorldMatrix(worldMatrix);
        frame.intensity = sanitizeLightIntensity(light.intensity);
        frame.color = sanitizeLightColor(light.color);
        frame.innerConeCos = std::cos(innerAngle);
        frame.outerConeCos = std::cos(outerAngle);
        return frame;
    }

    inline bool directionalLightRequiresSanitization(const DirectionalLightComponent& light)
    {
        return light.intensity < 0.0f
            || !std::isfinite(light.intensity)
            || light.ambientIntensity < 0.0f
            || !std::isfinite(light.ambientIntensity)
            || light.color.x < 0.0f || light.color.y < 0.0f || light.color.z < 0.0f
            || !std::isfinite(light.color.x) || !std::isfinite(light.color.y) ||
               !std::isfinite(light.color.z);
    }

    inline bool pointLightRequiresSanitization(const PointLightComponent& light)
    {
        return light.range < MinLightRange
            || !std::isfinite(light.range)
            || light.intensity < 0.0f
            || !std::isfinite(light.intensity)
            || light.color.x < 0.0f || light.color.y < 0.0f || light.color.z < 0.0f
            || !std::isfinite(light.color.x) || !std::isfinite(light.color.y) ||
               !std::isfinite(light.color.z);
    }

    inline bool spotLightRequiresSanitization(const SpotLightComponent& light)
    {
        float innerAngle = 0.0f;
        float outerAngle = 0.0f;
        sanitizeSpotConeAngles(light.innerConeAngle, light.outerConeAngle, innerAngle, outerAngle);
        return pointLightRequiresSanitization(PointLightComponent{
                light.color,
                light.intensity,
                light.range,
                light.enabled,
                light.priority
            })
            || !std::isfinite(light.innerConeAngle)
            || !std::isfinite(light.outerConeAngle)
            || std::abs(innerAngle - light.innerConeAngle) > 1.0e-6f
            || std::abs(outerAngle - light.outerConeAngle) > 1.0e-6f;
    }

    inline LightingFrameData extractLightingFrameData(ECSWorld& world,
                                                      const glm::vec3& activeCameraPosition)
    {
        struct RankedDirectional
        {
            Entity entity;
            int priority = 0;
        };

        struct RankedLocal
        {
            Entity entity;
            int priority = 0;
            float distanceSquared = 0.0f;
        };

        LightingFrameData frame = defaultLightingFrameData();
        std::vector<RankedDirectional> directionalCandidates;
        std::vector<RankedLocal> pointCandidates;
        std::vector<RankedLocal> spotCandidates;

        for (Entity entity : world.getAllEntitiesWith<DirectionalLightComponent>())
        {
            frame.diagnostics.totalDirectionalLights += 1;
            const DirectionalLightComponent& light = world.getComponent<DirectionalLightComponent>(entity);
            if (directionalLightRequiresSanitization(light))
                frame.diagnostics.sanitizedLights += 1;
            if (!light.enabled || !light.active)
                continue;

            directionalCandidates.push_back({entity, light.priority});
        }

        for (Entity entity : world.getAllEntitiesWith<PointLightComponent>())
        {
            frame.diagnostics.totalPointLights += 1;
            const PointLightComponent& light = world.getComponent<PointLightComponent>(entity);
            if (pointLightRequiresSanitization(light))
                frame.diagnostics.sanitizedLights += 1;
            if (!light.enabled)
                continue;

            const glm::vec3 position = positionFromWorldMatrix(resolvedLightWorldMatrix(world, entity));
            pointCandidates.push_back({
                entity,
                light.priority,
                glm::dot(position - activeCameraPosition, position - activeCameraPosition)
            });
        }

        for (Entity entity : world.getAllEntitiesWith<SpotLightComponent>())
        {
            frame.diagnostics.totalSpotLights += 1;
            const SpotLightComponent& light = world.getComponent<SpotLightComponent>(entity);
            if (spotLightRequiresSanitization(light))
                frame.diagnostics.sanitizedLights += 1;
            if (!light.enabled)
                continue;

            const glm::vec3 position = positionFromWorldMatrix(resolvedLightWorldMatrix(world, entity));
            spotCandidates.push_back({
                entity,
                light.priority,
                glm::dot(position - activeCameraPosition, position - activeCameraPosition)
            });
        }

        std::sort(
            directionalCandidates.begin(),
            directionalCandidates.end(),
            [](const RankedDirectional& lhs, const RankedDirectional& rhs)
            {
                if (lhs.priority != rhs.priority)
                    return lhs.priority > rhs.priority;
                return lhs.entity.id < rhs.entity.id;
            });

        auto localLightLess = [](const RankedLocal& lhs, const RankedLocal& rhs)
        {
            if (lhs.priority != rhs.priority)
                return lhs.priority > rhs.priority;
            if (lhs.distanceSquared != rhs.distanceSquared)
                return lhs.distanceSquared < rhs.distanceSquared;
            return lhs.entity.id < rhs.entity.id;
        };
        std::sort(pointCandidates.begin(), pointCandidates.end(), localLightLess);
        std::sort(spotCandidates.begin(), spotCandidates.end(), localLightLess);

        frame.directionalCount =
            std::min<uint32_t>(static_cast<uint32_t>(directionalCandidates.size()), MaxDirectionalLights);
        frame.pointCount =
            std::min<uint32_t>(static_cast<uint32_t>(pointCandidates.size()), MaxPointLights);
        frame.spotCount =
            std::min<uint32_t>(static_cast<uint32_t>(spotCandidates.size()), MaxSpotLights);
        frame.diagnostics.droppedDirectionalLights =
            static_cast<uint32_t>(directionalCandidates.size()) - frame.directionalCount;
        frame.diagnostics.droppedPointLights =
            static_cast<uint32_t>(pointCandidates.size()) - frame.pointCount;
        frame.diagnostics.droppedSpotLights =
            static_cast<uint32_t>(spotCandidates.size()) - frame.spotCount;

        for (uint32_t i = 0; i < frame.directionalCount; ++i)
        {
            Entity entity = directionalCandidates[i].entity;
            frame.directional[i] = makeDirectionalLightFrameData(
                world.getComponent<DirectionalLightComponent>(entity),
                resolvedLightWorldMatrix(world, entity));
        }
        if (frame.directionalCount > 0)
        {
            const DirectionalLightComponent& primary =
                world.getComponent<DirectionalLightComponent>(directionalCandidates[0].entity);
            frame.ambientIntensity = std::isfinite(primary.ambientIntensity)
                ? std::max(0.0f, primary.ambientIntensity)
                : 0.12f;
        }

        for (uint32_t i = 0; i < frame.pointCount; ++i)
        {
            Entity entity = pointCandidates[i].entity;
            frame.point[i] = makePointLightFrameData(
                world.getComponent<PointLightComponent>(entity),
                resolvedLightWorldMatrix(world, entity));
        }

        for (uint32_t i = 0; i < frame.spotCount; ++i)
        {
            Entity entity = spotCandidates[i].entity;
            frame.spot[i] = makeSpotLightFrameData(
                world.getComponent<SpotLightComponent>(entity),
                resolvedLightWorldMatrix(world, entity));
        }

        return frame;
    }

    inline DirectionalLightFrameData extractDirectionalLightFrameData(ECSWorld& world)
    {
        const LightingFrameData frame = extractLightingFrameData(world, {0.0f, 0.0f, 0.0f});
        return frame.directionalCount > 0
            ? frame.directional[0]
            : defaultDirectionalLightFrameData();
    }
}
