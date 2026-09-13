#pragma once

#include <cmath>

#include "ECSSimulationSystem.hpp"
#include "ECSWorld.hpp"
#include "TransformComponent.h"
#include "TransformDirtyHelpers.h"
#include "TransformAnimationComponent.h"
#include "TransformAnimationMode.h"

// adjusted to operate on individual entities instead of globally,
// likely needs further expansion in the future, but for simple transformations, this works well enough
class TransformAnimationSystem : public ECSSimulationSystem
{
    public:
    void update(const EcsSimulationContext& ctx) override
    {
        ctx.world.forEach<TransformComponent, TransformAnimationComponent>(
            [&](Entity entity, TransformComponent& transform, TransformAnimationComponent& anim)
            {
                if (!anim.enabled)
                    return;

                // Initialize initial transform if first frame
                if (!anim.initialized)
                {
                    anim.initialPosition = transform.position;
                    anim.initialRotation = transform.rotation;
                    anim.initialScale    = transform.scale;
                    anim.initialized     = true;
                }

                anim.time += ctx.dt;

                const glm::vec3 previousPosition = transform.position;
                const glm::vec3 previousRotation = transform.rotation;
                const glm::vec3 previousScale    = transform.scale;

                // Translation
                if (anim.hasMode(TransformAnimationMode::Translate))
                {
                    float offset       = std::sin(anim.time * anim.translationSpeed) * anim.translationAmplitude;
                    transform.position = anim.initialPosition + offset * anim.translationAxis;
                }

                // Rotation
                if (anim.hasMode(TransformAnimationMode::Rotate))
                {
                    transform.rotation =
                        anim.initialRotation + anim.rotationEulerFactors * anim.rotationSpeed * anim.time;
                }

                // Scaling
                if (anim.hasMode(TransformAnimationMode::Scale))
                {
                    float factor    = std::sin(anim.time * anim.scaleSpeed);
                    transform.scale = anim.initialScale + anim.scaleAmplitude * factor;
                }

                if (transform.position != previousPosition || transform.rotation != previousRotation ||
                    transform.scale != previousScale)
                {
                    gts::transform::markDirty(ctx.world, entity);
                }
            });
    }
};
