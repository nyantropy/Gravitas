#pragma once

#include <cmath>

#include "ECSSimulationSystem.hpp"
#include "TransformComponent.h"
#include "TransformDirtyHelpers.h"
#include "AnimationComponent.h"
#include "AnimationMode.h"

// adjusted to operate on individual entities instead of globally,
// likely needs further expansion in the future, but for simple transformations, this works well enough
class TransformAnimationSystem : public ECSSimulationSystem
{
    public:
        void update(const EcsSimulationContext& ctx) override
        {
            for (Entity e : ctx.world.getAllEntitiesWith<TransformComponent, AnimationComponent>())
            {
                auto& transform = ctx.world.getComponent<TransformComponent>(e);
                auto& anim = ctx.world.getComponent<AnimationComponent>(e);

                if (!anim.enabled)
                    continue;

                // Initialize initial transform if first frame
                if (!anim.initialized)
                {
                    anim.initialPosition = transform.position;
                    anim.initialRotation = transform.rotation;
                    anim.initialScale    = transform.scale;
                    anim.initialized = true;
                }

                anim.time += ctx.dt;

                // Translation
                if (anim.hasMode(AnimationMode::Translate))
                {
                    float offset = std::sin(anim.time * anim.translationSpeed) * anim.translationAmplitude;
                    transform.position = anim.initialPosition + offset * anim.translationAxis;
                    gts::transform::markDirty(ctx.world, e);
                }

                // Rotation
                if (anim.hasMode(AnimationMode::Rotate))
                {
                    transform.rotation = anim.initialRotation + anim.rotationAxis * anim.rotationSpeed * anim.time;
                    gts::transform::markDirty(ctx.world, e);
                }

                // Scaling
                if (anim.hasMode(AnimationMode::Scale))
                {
                    float factor = 1.0f + std::sin(anim.time * anim.scaleSpeed);
                    transform.scale = anim.initialScale + anim.scaleAmplitude * factor;
                    gts::transform::markDirty(ctx.world, e);
                }
            }
        }
};
