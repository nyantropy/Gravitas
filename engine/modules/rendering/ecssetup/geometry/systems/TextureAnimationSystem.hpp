#pragma once

#include <algorithm>
#include <cmath>

#include "ECSControllerSystem.hpp"
#include "RenderDirtyComponent.h"
#include "RenderGpuComponent.h"
#include "RenderInvalidationLifecycle.h"
#include "TextureAnimationComponent.h"
#include "TextureAnimationRuntimeComponent.h"
#include "TimeContext.h"

class TextureAnimationSystem : public ECSControllerSystem
{
public:
    void update(const EcsControllerContext& ctx) override
    {
        if (ctx.time == nullptr)
            return;

        ctx.world.forEach<TextureAnimationComponent,
                          TextureAnimationRuntimeComponent,
                          RenderGpuComponent,
                          RenderDirtyComponent>(
            [&](Entity entity,
                TextureAnimationComponent& animation,
                TextureAnimationRuntimeComponent& runtime,
                RenderGpuComponent& renderGpu,
                RenderDirtyComponent& dirty)
            {
                if (renderGpu.objectSSBOSlot == RENDERABLE_SLOT_UNALLOCATED || !renderGpu.readyToRender)
                    return;

                if (descriptorChanged(animation, runtime))
                {
                    runtime.elapsedSeconds = 0.0f;
                    runtime.lastDescriptor = animation;
                    runtime.hasLastDescriptor = true;
                }

                if (!animation.enabled)
                {
                    applyUvTransform(ctx, entity, renderGpu, dirty, identityUvTransform());
                    runtime.elapsedSeconds = 0.0f;
                    return;
                }

                runtime.elapsedSeconds += animationDt(animation, *ctx.time);
                applyUvTransform(ctx,
                                 entity,
                                 renderGpu,
                                 dirty,
                                 computeUvTransform(animation, runtime.elapsedSeconds));
            });
    }

private:
    static glm::vec4 identityUvTransform()
    {
        return {1.0f, 1.0f, 0.0f, 0.0f};
    }

    static float wrapUv(float value)
    {
        return value - std::floor(value);
    }

    static glm::vec2 wrapUv(const glm::vec2& value)
    {
        return {wrapUv(value.x), wrapUv(value.y)};
    }

    static glm::vec4 computeUvTransform(const TextureAnimationComponent& animation,
                                        float elapsedSeconds)
    {
        const float time = elapsedSeconds + animation.phaseSeconds;
        const glm::vec2 animatedOffset = animation.uvOffset + animation.scrollSpeed * time;

        if (animation.mode == TextureAnimationMode::FlipbookAtlas)
        {
            const uint32_t columns = std::max(1u, animation.columns);
            const uint32_t rows = std::max(1u, animation.rows);
            const uint32_t maxFrames = columns * rows;
            const uint32_t frameCount = std::clamp(animation.frameCount, 1u, maxFrames);
            uint32_t frame = 0;

            if (frameCount > 1 && animation.frameRate > 0.0f)
            {
                const float framePosition =
                    (elapsedSeconds + animation.phaseSeconds) * animation.frameRate;
                if (animation.loop)
                {
                    frame = static_cast<uint32_t>(std::floor(framePosition))
                        % frameCount;
                }
                else
                {
                    frame = std::min(static_cast<uint32_t>(std::floor(framePosition)),
                                     frameCount - 1u);
                }
            }

            const uint32_t column = frame % columns;
            const uint32_t row = frame / columns;
            const glm::vec2 atlasScale = {
                1.0f / static_cast<float>(columns),
                1.0f / static_cast<float>(rows)
            };

            const glm::vec2 cellOffset = {
                static_cast<float>(column) * atlasScale.x,
                static_cast<float>(row) * atlasScale.y
            };

            const glm::vec2 localOffset = wrapUv(animatedOffset);
            const glm::vec2 scale = animation.uvScale * atlasScale;
            const glm::vec2 offset = cellOffset + localOffset * atlasScale;
            return {scale.x, scale.y, offset.x, offset.y};
        }

        const glm::vec2 scale = animation.uvScale;
        const glm::vec2 offset = wrapUv(animatedOffset);
        return {scale.x, scale.y, offset.x, offset.y};
    }

    static bool differs(const glm::vec4& lhs, const glm::vec4& rhs)
    {
        return lhs.x != rhs.x || lhs.y != rhs.y || lhs.z != rhs.z || lhs.w != rhs.w;
    }

    static bool differs(const glm::vec2& lhs, const glm::vec2& rhs)
    {
        return lhs.x != rhs.x || lhs.y != rhs.y;
    }

    static bool descriptorChanged(const TextureAnimationComponent& animation,
                                  const TextureAnimationRuntimeComponent& runtime)
    {
        if (!runtime.hasLastDescriptor)
            return true;

        const auto& last = runtime.lastDescriptor;
        return animation.enabled != last.enabled
            || animation.mode != last.mode
            || animation.timeMode != last.timeMode
            || differs(animation.uvScale, last.uvScale)
            || differs(animation.uvOffset, last.uvOffset)
            || differs(animation.scrollSpeed, last.scrollSpeed)
            || animation.columns != last.columns
            || animation.rows != last.rows
            || animation.frameCount != last.frameCount
            || animation.frameRate != last.frameRate
            || animation.loop != last.loop
            || animation.phaseSeconds != last.phaseSeconds;
    }

    static float animationDt(const TextureAnimationComponent& animation,
                             const TimeContext& time)
    {
        const float unscaled = std::max(0.0f, time.unscaledDeltaTime);
        if (animation.timeMode == TextureAnimationTimeMode::Unscaled)
            return unscaled;

        if (time.deltaTime <= 0.0f)
            return 0.0f;

        return unscaled * std::max(0.0f, time.timeScale);
    }

    static void applyUvTransform(const EcsControllerContext& ctx,
                                 Entity entity,
                                 RenderGpuComponent& renderGpu,
                                 RenderDirtyComponent& dirty,
                                 const glm::vec4& uvTransform)
    {
        if (!differs(renderGpu.uvTransform, uvTransform) && !dirty.objectDataDirty)
            return;

        renderGpu.uvTransform = uvTransform;
        dirty.objectDataDirty = true;
        gts::rendering::queueRenderSnapshotDirty(ctx.world, entity);
    }
};
