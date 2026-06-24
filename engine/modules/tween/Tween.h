#pragma once

#include <algorithm>
#include <cmath>

namespace gts::tween
{
    enum class TweenEase
    {
        Linear = 0,
        EaseInQuad,
        EaseOutQuad,
        EaseInOutQuad,
        SmoothStep
    };

    inline float applyEase(TweenEase ease, float t)
    {
        t = std::clamp(t, 0.0f, 1.0f);

        switch (ease)
        {
            case TweenEase::EaseInQuad:
                return t * t;
            case TweenEase::EaseOutQuad:
                return 1.0f - (1.0f - t) * (1.0f - t);
            case TweenEase::EaseInOutQuad:
                return t < 0.5f
                    ? 2.0f * t * t
                    : 1.0f - std::pow(-2.0f * t + 2.0f, 2.0f) * 0.5f;
            case TweenEase::SmoothStep:
                return t * t * (3.0f - 2.0f * t);
            case TweenEase::Linear:
            default:
                return t;
        }
    }

    template <typename T>
    inline T lerp(const T& from, const T& to, float t)
    {
        return from + (to - from) * t;
    }

    template <typename T>
    class Tween
    {
    public:
        void start(const T& from, const T& to, float seconds, TweenEase inEase = TweenEase::SmoothStep)
        {
            fromValue = from;
            toValue = to;
            currentValue = from;
            durationSeconds = std::max(0.0f, seconds);
            elapsedSeconds = 0.0f;
            ease = inEase;
            active = durationSeconds > 0.0f;
            completed = !active;

            if (!active)
                currentValue = toValue;
        }

        bool update(float dt)
        {
            if (!active)
                return false;

            elapsedSeconds = std::min(durationSeconds, elapsedSeconds + std::max(0.0f, dt));
            const float normalized = durationSeconds <= 0.0f ? 1.0f : elapsedSeconds / durationSeconds;
            currentValue = lerp(fromValue, toValue, applyEase(ease, normalized));

            if (elapsedSeconds < durationSeconds)
                return false;

            currentValue = toValue;
            active = false;
            completed = true;
            return true;
        }

        void finish()
        {
            currentValue = toValue;
            elapsedSeconds = durationSeconds;
            active = false;
            completed = true;
        }

        void clear()
        {
            *this = Tween<T>{};
        }

        const T& value() const
        {
            return currentValue;
        }

        const T& target() const
        {
            return toValue;
        }

        float elapsed() const
        {
            return elapsedSeconds;
        }

        float duration() const
        {
            return durationSeconds;
        }

        float progress() const
        {
            if (durationSeconds <= 0.0f)
                return 1.0f;

            return std::clamp(elapsedSeconds / durationSeconds, 0.0f, 1.0f);
        }

        bool isActive() const
        {
            return active;
        }

        bool isComplete() const
        {
            return completed;
        }

    private:
        T fromValue{};
        T toValue{};
        T currentValue{};
        float durationSeconds = 0.0f;
        float elapsedSeconds = 0.0f;
        TweenEase ease = TweenEase::SmoothStep;
        bool active = false;
        bool completed = true;
    };
} // namespace gts::tween
