#pragma once

#include <algorithm>
#include <cmath>

#include "ECSControllerSystem.hpp"
#include "RenderDirtyComponent.h"
#include "RenderGpuComponent.h"
#include "RenderInvalidationLifecycle.h"
#include "TextureAnimationComponent.h"
#include "TextureAnimationRuntimeComponent.h"

class TextureAnimationSystem : public ECSControllerSystem
{
public:
    void update(const EcsControllerContext& ctx) override
    {
        if (ctx.time == nullptr)
            return;

        auto& commands = ctx.world.commands();

        ctx.world.forEachSnapshot<TextureAnimationComponent>(
            [&](Entity entity, TextureAnimationComponent&)
            {
                if (!ctx.world.hasComponent<TextureAnimationRuntimeComponent>(entity))
                    commands.addComponent<TextureAnimationRuntimeComponent>(
                        entity, TextureAnimationRuntimeComponent{});
            });

        ctx.world.forEachSnapshot<TextureAnimationRuntimeComponent>(
            [&](Entity entity, TextureAnimationRuntimeComponent&)
            {
                if (!ctx.world.hasComponent<TextureAnimationComponent>(entity))
                    commands.removeComponent<TextureAnimationRuntimeComponent>(entity);
            });

        const float dt = std::max(0.0f, ctx.time->unscaledDeltaTime);
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

                if (!animation.enabled || animation.mode == TextureAnimationMode::None)
                {
                    applyUvTransform(ctx, entity, renderGpu, dirty, identityUvTransform());
                    runtime.elapsedSeconds = 0.0f;
                    return;
                }

                runtime.elapsedSeconds += dt;
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

    static glm::vec4 computeUvTransform(const TextureAnimationComponent& animation,
                                        float elapsedSeconds)
    {
        glm::vec2 scale = animation.uvScale;
        glm::vec2 offset = animation.uvOffset
            + animation.scrollSpeed * (elapsedSeconds + animation.phaseSeconds);

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

            scale *= atlasScale;
            offset += glm::vec2{
                static_cast<float>(column) * atlasScale.x,
                static_cast<float>(row) * atlasScale.y
            };
        }

        offset.x = offset.x - std::floor(offset.x);
        offset.y = offset.y - std::floor(offset.y);
        return {scale.x, scale.y, offset.x, offset.y};
    }

    static bool differs(const glm::vec4& lhs, const glm::vec4& rhs)
    {
        return lhs.x != rhs.x || lhs.y != rhs.y || lhs.z != rhs.z || lhs.w != rhs.w;
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
