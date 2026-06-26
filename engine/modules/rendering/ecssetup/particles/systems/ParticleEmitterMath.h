#pragma once

#include <algorithm>
#include <cstddef>
#include <cmath>
#include <limits>

#include "ParticleEmitterComponent.h"
#include "ParticleEmitterRuntimeComponent.h"

struct ParticleEmitterMath
{
    static constexpr float Pi = 3.14159265358979323846f;

    static uint32_t nextRandom(uint32_t& state)
    {
        if (state == 0u)
            state = 1u;

        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return state;
    }

    static float random01(uint32_t& state)
    {
        constexpr float invMax = 1.0f / static_cast<float>(std::numeric_limits<uint32_t>::max());
        return static_cast<float>(nextRandom(state)) * invMax;
    }

    static float randomRange(uint32_t& state, float minValue, float maxValue)
    {
        if (maxValue <= minValue)
            return minValue;
        return minValue + (maxValue - minValue) * random01(state);
    }

    static glm::vec3 randomUnitVector(uint32_t& state)
    {
        const float z = randomRange(state, -1.0f, 1.0f);
        const float a = randomRange(state, 0.0f, Pi * 2.0f);
        const float r = std::sqrt(std::max(0.0f, 1.0f - z * z));
        return {r * std::cos(a), z, r * std::sin(a)};
    }

    static glm::vec3 randomInDisc(uint32_t& state, float minRadius, float maxRadius)
    {
        const float radius = std::sqrt(random01(state))
            * std::max(0.0f, maxRadius - minRadius)
            + std::max(0.0f, minRadius);
        const float angle = randomRange(state, 0.0f, Pi * 2.0f);
        return {std::cos(angle) * radius, 0.0f, std::sin(angle) * radius};
    }

    static glm::vec3 sampleShape(const ParticleEmitterComponent& emitter, uint32_t& rng)
    {
        switch (emitter.shape)
        {
            case ParticleEmitterShape::Sphere:
            {
                const float radius = std::cbrt(random01(rng)) * std::max(0.0f, emitter.sphereRadius);
                return randomUnitVector(rng) * radius;
            }
            case ParticleEmitterShape::Box:
                return {
                    randomRange(rng, -emitter.boxExtents.x, emitter.boxExtents.x),
                    randomRange(rng, -emitter.boxExtents.y, emitter.boxExtents.y),
                    randomRange(rng, -emitter.boxExtents.z, emitter.boxExtents.z)
                };
            case ParticleEmitterShape::Disc:
                return randomInDisc(rng, 0.0f, emitter.discRadius);
            case ParticleEmitterShape::Cylinder:
            {
                glm::vec3 p = randomInDisc(rng, 0.0f, emitter.cylinderRadius);
                p.y = randomRange(rng, -emitter.cylinderHeight * 0.5f, emitter.cylinderHeight * 0.5f);
                return p;
            }
            case ParticleEmitterShape::Ring:
                return randomInDisc(rng, emitter.ringInnerRadius, emitter.ringOuterRadius);
        }

        return {0.0f, 0.0f, 0.0f};
    }

    static glm::vec3 radialDirection(const glm::vec3& localPosition)
    {
        glm::vec3 flat = {localPosition.x, 0.0f, localPosition.z};
        if (glm::length(flat) > 0.0001f)
            return glm::normalize(flat);

        if (glm::length(localPosition) > 0.0001f)
            return glm::normalize(localPosition);

        return {0.0f, 1.0f, 0.0f};
    }

    static glm::vec3 tangentDirection(const glm::vec3& radial)
    {
        glm::vec3 tangent = {-radial.z, 0.0f, radial.x};
        if (glm::length(tangent) > 0.0001f)
            return glm::normalize(tangent);
        return {1.0f, 0.0f, 0.0f};
    }

    static glm::vec3 rgbToHsv(const glm::vec3& rgb)
    {
        const float cMax = std::max(rgb.r, std::max(rgb.g, rgb.b));
        const float cMin = std::min(rgb.r, std::min(rgb.g, rgb.b));
        const float delta = cMax - cMin;

        glm::vec3 hsv{0.0f, 0.0f, cMax};
        if (delta <= 0.00001f)
            return hsv;

        hsv.y = cMax <= 0.0f ? 0.0f : delta / cMax;
        if (cMax == rgb.r)
            hsv.x = std::fmod((rgb.g - rgb.b) / delta, 6.0f);
        else if (cMax == rgb.g)
            hsv.x = ((rgb.b - rgb.r) / delta) + 2.0f;
        else
            hsv.x = ((rgb.r - rgb.g) / delta) + 4.0f;

        hsv.x /= 6.0f;
        if (hsv.x < 0.0f)
            hsv.x += 1.0f;
        return hsv;
    }

    static glm::vec3 hsvToRgb(const glm::vec3& hsv)
    {
        const float h = hsv.x * 6.0f;
        const float c = hsv.z * hsv.y;
        const float x = c * (1.0f - std::abs(std::fmod(h, 2.0f) - 1.0f));
        const float m = hsv.z - c;

        glm::vec3 rgb{0.0f, 0.0f, 0.0f};
        if (h < 1.0f)      rgb = {c, x, 0.0f};
        else if (h < 2.0f) rgb = {x, c, 0.0f};
        else if (h < 3.0f) rgb = {0.0f, c, x};
        else if (h < 4.0f) rgb = {0.0f, x, c};
        else if (h < 5.0f) rgb = {x, 0.0f, c};
        else               rgb = {c, 0.0f, x};
        return rgb + glm::vec3(m);
    }

    static glm::vec4 applyColorVariation(const glm::vec4& color,
                                         float hueVariation,
                                         float valueVariation,
                                         uint32_t& rng)
    {
        glm::vec3 hsv = rgbToHsv(glm::clamp(glm::vec3(color), glm::vec3(0.0f), glm::vec3(1.0f)));
        hsv.x += randomRange(rng, -hueVariation, hueVariation);
        hsv.x = hsv.x - std::floor(hsv.x);
        hsv.z = glm::clamp(hsv.z + randomRange(rng, -valueVariation, valueVariation), 0.0f, 1.0f);
        return {hsvToRgb(hsv), color.a};
    }

    static float evaluateFloatCurve(const ParticleFloatCurve& curve, float t, float fallback)
    {
        if (curve.empty())
            return fallback;

        if (t <= curve.front().t)
            return curve.front().value;

        for (size_t i = 1; i < curve.size(); ++i)
        {
            if (t > curve[i].t)
                continue;

            const ParticleFloatKey& a = curve[i - 1];
            const ParticleFloatKey& b = curve[i];
            const float span = std::max(0.0001f, b.t - a.t);
            const float localT = glm::clamp((t - a.t) / span, 0.0f, 1.0f);
            return glm::mix(a.value, b.value, localT);
        }

        return curve.back().value;
    }

    static glm::vec4 evaluateColorCurve(const ParticleColorCurve& curve, float t)
    {
        if (curve.empty())
            return {1.0f, 1.0f, 1.0f, 1.0f};

        if (t <= curve.front().t)
            return curve.front().color;

        for (size_t i = 1; i < curve.size(); ++i)
        {
            if (t > curve[i].t)
                continue;

            const ParticleColorKey& a = curve[i - 1];
            const ParticleColorKey& b = curve[i];
            const float span = std::max(0.0001f, b.t - a.t);
            const float localT = glm::clamp((t - a.t) / span, 0.0f, 1.0f);
            return glm::mix(a.color, b.color, localT);
        }

        return curve.back().color;
    }

    static glm::vec4 computeFlipbookUv(const ParticleEmitterComponent& emitter,
                                       const ParticleState& particle,
                                       float normalizedAge)
    {
        const uint32_t columns = std::max(1u, emitter.flipbook.columns);
        const uint32_t rows = std::max(1u, emitter.flipbook.rows);
        const uint32_t maxFrames = columns * rows;
        const uint32_t frameCount = glm::clamp(emitter.flipbook.frameCount, 1u, maxFrames);
        uint32_t frame = 0;

        if (frameCount > 1)
        {
            if (emitter.flipbook.lifetimeDriven)
            {
                frame = static_cast<uint32_t>(
                    glm::clamp(normalizedAge, 0.0f, 0.9999f) * static_cast<float>(frameCount));
            }
            else
            {
                const float animated =
                    particle.age * std::max(0.0f, emitter.flipbook.frameRate) + particle.frameOffset;
                frame = static_cast<uint32_t>(animated) % frameCount;
            }
        }

        const uint32_t column = frame % columns;
        const uint32_t row = frame / columns;
        const float invColumns = 1.0f / static_cast<float>(columns);
        const float invRows = 1.0f / static_cast<float>(rows);
        const float u0 = static_cast<float>(column) * invColumns;
        const float v0 = static_cast<float>(row) * invRows;
        return {u0, v0, u0 + invColumns, v0 + invRows};
    }
};
