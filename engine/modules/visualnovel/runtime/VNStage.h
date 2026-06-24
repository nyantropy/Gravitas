#pragma once

#include <algorithm>
#include <cmath>
#include <string>
#include <unordered_map>
#include <vector>

#include "VNTypes.h"

namespace gts::vn
{
    class VNStage
    {
    public:
        void clear()
        {
            background = {};
            sprites.clear();
            dimAlpha = 0.0f;
            dimTween.clear();
        }

        void useCurrentSceneAsBackground()
        {
            VNBackground next;
            next.mode = VNBackgroundMode::UseCurrentScene;
            background = next;
        }

        void useImageBackground(const std::string& imageAsset)
        {
            VNBackground next;
            next.mode = VNBackgroundMode::FullscreenImage;
            next.imageAsset = imageAsset;
            background = next;
        }

        void useSolidBackground(const glm::vec4& color)
        {
            VNBackground next;
            next.mode = VNBackgroundMode::SolidColor;
            next.color = color;
            background = next;
        }

        void clearBackgroundOverride()
        {
            VNBackground next;
            next.mode = VNBackgroundMode::UseCurrentScene;
            background = next;
        }

        void setBackground(const VNBackground& inBackground)
        {
            background = inBackground;
        }

        const VNBackground& getBackground() const
        {
            return background;
        }

        void setDimming(float alpha, float durationSeconds = 0.0f, gts::tween::TweenEase ease = gts::tween::TweenEase::SmoothStep)
        {
            alpha = std::clamp(alpha, 0.0f, 1.0f);
            if (durationSeconds <= 0.0f)
            {
                dimAlpha = alpha;
                dimTween.clear();
                return;
            }

            dimTween.start(dimAlpha, alpha, durationSeconds, ease);
        }

        float getDimming() const
        {
            return dimAlpha;
        }

        void showSprite(const std::string& id,
                        const VNSprite& sprite,
                        float durationSeconds = 0.0f,
                        gts::tween::TweenEase ease = gts::tween::TweenEase::SmoothStep)
        {
            VNSpriteRuntime& runtime = sprites[id];
            runtime.sprite = sprite;
            runtime.sprite.visible = true;
            runtime.hideAfterFade = false;

            if (durationSeconds > 0.0f)
            {
                const float targetAlpha = std::clamp(sprite.alpha, 0.0f, 1.0f);
                runtime.sprite.alpha = 0.0f;
                runtime.alpha.start(0.0f, targetAlpha, durationSeconds, ease);
            }
        }

        void hideSprite(const std::string& id,
                        float durationSeconds = 0.0f,
                        gts::tween::TweenEase ease = gts::tween::TweenEase::SmoothStep)
        {
            auto it = sprites.find(id);
            if (it == sprites.end())
                return;

            VNSpriteRuntime& runtime = it->second;
            if (durationSeconds <= 0.0f)
            {
                runtime.sprite.visible = false;
                runtime.sprite.alpha = 0.0f;
                runtime.hideAfterFade = false;
                runtime.alpha.clear();
                return;
            }

            runtime.hideAfterFade = true;
            runtime.alpha.start(runtime.sprite.alpha, 0.0f, durationSeconds, ease);
        }

        void moveSprite(const std::string& id,
                        const glm::vec2& position,
                        float durationSeconds,
                        gts::tween::TweenEase ease = gts::tween::TweenEase::SmoothStep)
        {
            VNSpriteRuntime& runtime = sprites[id];
            if (durationSeconds <= 0.0f)
            {
                runtime.sprite.position = position;
                runtime.position.clear();
                return;
            }

            runtime.position.start(runtime.sprite.position, position, durationSeconds, ease);
        }

        void scaleSprite(const std::string& id,
                         const glm::vec2& scale,
                         float durationSeconds,
                         gts::tween::TweenEase ease = gts::tween::TweenEase::SmoothStep)
        {
            VNSpriteRuntime& runtime = sprites[id];
            if (durationSeconds <= 0.0f)
            {
                runtime.sprite.scale = scale;
                runtime.scale.clear();
                return;
            }

            runtime.scale.start(runtime.sprite.scale, scale, durationSeconds, ease);
        }

        void fadeSprite(const std::string& id,
                        float alpha,
                        float durationSeconds,
                        gts::tween::TweenEase ease = gts::tween::TweenEase::SmoothStep)
        {
            VNSpriteRuntime& runtime = sprites[id];
            alpha = std::clamp(alpha, 0.0f, 1.0f);
            runtime.sprite.visible = true;
            runtime.hideAfterFade = false;

            if (durationSeconds <= 0.0f)
            {
                runtime.sprite.alpha = alpha;
                runtime.alpha.clear();
                return;
            }

            runtime.alpha.start(runtime.sprite.alpha, alpha, durationSeconds, ease);
        }

        void rotateSprite(const std::string& id,
                          float rotation,
                          float durationSeconds,
                          gts::tween::TweenEase ease = gts::tween::TweenEase::SmoothStep)
        {
            VNSpriteRuntime& runtime = sprites[id];
            if (durationSeconds <= 0.0f)
            {
                runtime.sprite.rotation = rotation;
                runtime.rotation.clear();
                return;
            }

            runtime.rotation.start(runtime.sprite.rotation, rotation, durationSeconds, ease);
        }

        void shakeSprite(const std::string& id,
                         float durationSeconds,
                         float amplitude = 0.012f,
                         float frequency = 18.0f,
                         gts::tween::TweenEase ease = gts::tween::TweenEase::EaseOutQuad)
        {
            VNSpriteRuntime& runtime = sprites[id];
            runtime.shakeAmplitude = std::max(0.0f, amplitude);
            runtime.shakeFrequency = std::max(0.0f, frequency);
            runtime.shake.start(0.0f, 1.0f, std::max(0.0f, durationSeconds), ease);
        }

        void changeExpression(const std::string& id,
                              const std::string& imageAsset,
                              const std::string& expression = {})
        {
            VNSpriteRuntime& runtime = sprites[id];
            if (!imageAsset.empty())
                runtime.sprite.imageAsset = imageAsset;
            runtime.sprite.expression = expression;
        }

        bool update(float dt)
        {
            bool anyActive = false;

            if (dimTween.isActive())
            {
                dimTween.update(dt);
                dimAlpha = dimTween.value();
                anyActive = anyActive || dimTween.isActive();
            }

            for (auto& [id, runtime] : sprites)
            {
                if (runtime.position.isActive())
                {
                    runtime.position.update(dt);
                    runtime.sprite.position = runtime.position.value();
                }
                if (runtime.scale.isActive())
                {
                    runtime.scale.update(dt);
                    runtime.sprite.scale = runtime.scale.value();
                }
                if (runtime.rotation.isActive())
                {
                    runtime.rotation.update(dt);
                    runtime.sprite.rotation = runtime.rotation.value();
                }
                if (runtime.alpha.isActive())
                {
                    const bool finished = runtime.alpha.update(dt);
                    runtime.sprite.alpha = runtime.alpha.value();
                    if (finished && runtime.hideAfterFade)
                    {
                        runtime.sprite.visible = false;
                        runtime.hideAfterFade = false;
                    }
                }
                if (runtime.shake.isActive())
                    runtime.shake.update(dt);

                anyActive = anyActive || runtime.position.isActive()
                    || runtime.scale.isActive()
                    || runtime.rotation.isActive()
                    || runtime.alpha.isActive()
                    || runtime.shake.isActive();
            }

            return anyActive;
        }

        bool hasActiveTweens() const
        {
            if (dimTween.isActive())
                return true;

            for (const auto& [id, runtime] : sprites)
            {
                if (runtime.position.isActive()
                    || runtime.scale.isActive()
                    || runtime.rotation.isActive()
                    || runtime.alpha.isActive()
                    || runtime.shake.isActive())
                {
                    return true;
                }
            }

            return false;
        }

        std::vector<VNSpriteView> spritesForRender() const
        {
            std::vector<VNSpriteView> result;
            result.reserve(sprites.size());

            for (const auto& [id, runtime] : sprites)
            {
                if (!runtime.sprite.visible || runtime.sprite.imageAsset.empty() || runtime.sprite.alpha <= 0.0f)
                    continue;

                VNSprite sprite = runtime.sprite;
                if (runtime.shake.isActive())
                {
                    constexpr float TwoPi = 6.28318530717958647692f;
                    const float progress = runtime.shake.progress();
                    const float envelope = 1.0f - progress;
                    const float wave = std::sin(progress * runtime.shakeFrequency * TwoPi);
                    sprite.position.x += wave * runtime.shakeAmplitude * envelope;
                }

                result.push_back(VNSpriteView{.id = id, .sprite = sprite});
            }

            std::sort(result.begin(),
                      result.end(),
                      [](const VNSpriteView& lhs, const VNSpriteView& rhs)
                      {
                          if (lhs.sprite.zOrder == rhs.sprite.zOrder)
                              return lhs.id < rhs.id;

                          return lhs.sprite.zOrder < rhs.sprite.zOrder;
                      });
            return result;
        }

    private:
        struct VNSpriteRuntime
        {
            VNSprite sprite;
            gts::tween::Tween<glm::vec2> position;
            gts::tween::Tween<glm::vec2> scale;
            gts::tween::Tween<float> rotation;
            gts::tween::Tween<float> alpha;
            gts::tween::Tween<float> shake;
            float shakeAmplitude = 0.012f;
            float shakeFrequency = 18.0f;
            bool hideAfterFade = false;
        };

        VNBackground background;
        std::unordered_map<std::string, VNSpriteRuntime> sprites;
        float dimAlpha = 0.0f;
        gts::tween::Tween<float> dimTween;
    };
} // namespace gts::vn
