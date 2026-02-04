#pragma once

#include <cmath>

#include "ECSSystem.hpp"
#include "TransformComponent.h"
#include "AnimationComponent.h"
#include "AnimationMode.h"

// adjusted to operate on individual entities instead of globally,
// likely needs further expansion in the future, but for simple transformations, this works well enough
class TransformAnimationSystem : public ECSSystem
{
    public:
        void update(ECSWorld& world, float dt) override
        {
            for (Entity e : world.getAllEntitiesWith<TransformComponent, AnimationComponent>())
            {
                auto& transform = world.getComponent<TransformComponent>(e);
                auto& anim = world.getComponent<AnimationComponent>(e);

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

                anim.time += dt;

                // Translation
                if (anim.hasMode(AnimationMode::Translate))
                {
                    float offset = std::sin(anim.time * anim.translationSpeed) * anim.translationAmplitude;
                    transform.position = anim.initialPosition + offset * anim.translationAxis;
                }

                // Rotation
                if (anim.hasMode(AnimationMode::Rotate))
                {
                    transform.rotation = anim.initialRotation + anim.rotationAxis * anim.rotationSpeed * anim.time;
                }

                // Scaling
                if (anim.hasMode(AnimationMode::Scale))
                {
                    float factor = 1.0f + std::sin(anim.time * anim.scaleSpeed);
                    transform.scale = anim.initialScale + anim.scaleAmplitude * factor;
                }
            }
        }
};

