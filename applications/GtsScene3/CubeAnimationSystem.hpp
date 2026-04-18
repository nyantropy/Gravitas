#pragma once

#include <cmath>

#include "BoundsComponent.h"
#include "CubeAnimationComponent.h"
#include "ECSSimulationSystem.hpp"
#include "TransformComponent.h"
#include "TransformDirtyHelpers.h"

class CubeAnimationSystem : public ECSSimulationSystem
{
public:
    void update(const EcsSimulationContext& ctx) override
    {
        ctx.world.forEach<CubeAnimationComponent, TransformComponent>(
            [&](Entity entity, CubeAnimationComponent& animation, TransformComponent& transform)
        {
            animation.accumulatedTime += ctx.dt;

            const float t = animation.accumulatedTime * animation.speed + animation.phase;
            glm::vec3 position = animation.homePosition;
            glm::vec3 rotation = glm::vec3(0.0f);

            switch (animation.animationType)
            {
                case 0:
                {
                    position.x += std::cos(t) * animation.orbitRadius;
                    position.z += std::sin(t) * animation.orbitRadius;
                    position.y += std::sin(t * 1.7f) * animation.amplitude * 0.35f;
                    rotation.y = t;
                    break;
                }
                case 1:
                {
                    const float bounce = std::sin(t) * animation.amplitude;
                    position.y += bounce;
                    rotation.x = bounce * 0.35f;
                    rotation.z = std::cos(t * 0.75f) * animation.rotationAmount * 0.25f;
                    break;
                }
                case 2:
                {
                    rotation = animation.axis * (t * animation.rotationAmount);
                    break;
                }
                case 3:
                default:
                {
                    const glm::vec3 secondaryAxis = secondary(animation.axis);
                    position += animation.axis * (std::sin(t) * animation.amplitude * 0.45f);
                    position += secondaryAxis * (std::cos(t * 1.35f) * animation.orbitRadius * 0.25f);
                    rotation = animation.axis * (std::sin(t * 0.8f) * animation.rotationAmount);
                    rotation += secondaryAxis * (std::cos(t * 1.1f) * animation.rotationAmount * 0.5f);
                    break;
                }
            }

            transform.position = position;
            transform.rotation = rotation;
            gts::transform::markDirty(ctx.world, entity);
        });
    }

private:
    static glm::vec3 normalizeOrDefault(const glm::vec3& value, const glm::vec3& fallback)
    {
        const float lenSq = glm::dot(value, value);
        if (lenSq <= 0.0001f)
            return fallback;

        return value / std::sqrt(lenSq);
    }

    static glm::vec3 secondary(const glm::vec3& axis)
    {
        glm::vec3 candidate = glm::cross(axis, glm::vec3(0.0f, 1.0f, 0.0f));
        if (glm::dot(candidate, candidate) <= 0.0001f)
            candidate = glm::cross(axis, glm::vec3(1.0f, 0.0f, 0.0f));

        return normalizeOrDefault(candidate, glm::vec3(1.0f, 0.0f, 0.0f));
    }
};
