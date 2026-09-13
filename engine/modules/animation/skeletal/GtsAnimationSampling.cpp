#include "GtsAnimationSampling.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "assets/animation/GtsAnimationTrack.h"

namespace gts::animation::detail
{
    namespace
    {
        glm::dvec4 xyzw(const glm::quat& q)
        {
            return {q.x, q.y, q.z, q.w};
        }

        std::optional<SampledValue> rotationValue(const glm::dvec4& value)
        {
            const double lengthSquared = glm::dot(value, value);
            if (!std::isfinite(lengthSquared) || lengthSquared <= 0.0)
                return std::nullopt;
            const auto unit = value / std::sqrt(lengthSquared);
            return glm::quat(static_cast<float>(unit.w),
                             static_cast<float>(unit.x),
                             static_cast<float>(unit.y),
                             static_cast<float>(unit.z));
        }

        std::optional<SampledValue> vectorValue(const glm::dvec3& value)
        {
            for (glm::length_t c = 0; c < 3; ++c)
                if (!std::isfinite(value[c]) || std::abs(value[c]) > std::numeric_limits<float>::max())
                    return std::nullopt;
            return glm::vec3(value);
        }

        SampledValue keyValue(const GtsAnimationTrack& track, size_t index)
        {
            if (track.interpolation == GtsAnimationInterpolation::CubicSpline)
            {
                if (track.target == GtsAnimationTarget::Rotation)
                    return std::get<std::vector<GtsAnimationCubicRotationKey>>(track.values)[index].value;
                return std::get<std::vector<GtsAnimationCubicVec3Key>>(track.values)[index].value;
            }
            if (track.target == GtsAnimationTarget::Rotation)
                return std::get<std::vector<glm::quat>>(track.values)[index];
            return std::get<std::vector<glm::vec3>>(track.values)[index];
        }

        template <class Vector>
        Vector hermite(
            const Vector& p0, const Vector& outTangent, const Vector& p1, const Vector& inTangent, double u, double dt)
        {
            const double u2 = u * u;
            const double u3 = u2 * u;
            return (2 * u3 - 3 * u2 + 1) * p0 + (u3 - 2 * u2 + u) * dt * outTangent + (-2 * u3 + 3 * u2) * p1 +
                   (u3 - u2) * dt * inTangent;
        }
    } // namespace

    std::optional<SampledValue> sampleTrack(const GtsAnimationTrack& track, float timeSeconds)
    {
        const auto& times = track.timesSeconds;
        const auto  next  = std::lower_bound(times.begin(), times.end(), timeSeconds);
        if (next == times.end())
            return keyValue(track, times.size() - 1);
        const size_t right = static_cast<size_t>(next - times.begin());
        if (next == times.begin() || *next == timeSeconds)
            return keyValue(track, right);
        const size_t left = right - 1;
        if (track.interpolation == GtsAnimationInterpolation::Step)
            return keyValue(track, left);

        // double intermediates avoid overflow in dt and in finite authored derivatives
        const double dt = static_cast<double>(times[right]) - times[left];
        const double u  = (static_cast<double>(timeSeconds) - times[left]) / dt;
        if (track.interpolation == GtsAnimationInterpolation::Linear)
        {
            if (track.target == GtsAnimationTarget::Rotation)
            {
                const auto& values = std::get<std::vector<glm::quat>>(track.values);
                const auto  a      = glm::normalize(glm::dquat(values[left]));
                const auto  b      = glm::normalize(glm::dquat(values[right]));
                // GLM slerp adjusts a temporary endpoint sign for the shortest arc
                const auto sampled = glm::slerp(a, b, u);
                return rotationValue({sampled.x, sampled.y, sampled.z, sampled.w});
            }
            const auto& values = std::get<std::vector<glm::vec3>>(track.values);
            return vectorValue((1 - u) * glm::dvec3(values[left]) + u * glm::dvec3(values[right]));
        }
        if (track.target == GtsAnimationTarget::Rotation)
        {
            const auto& values = std::get<std::vector<GtsAnimationCubicRotationKey>>(track.values);
            const auto& a      = values[left];
            const auto& b      = values[right];
            // cubic signs and derivatives remain authored; only the result is normalized
            return rotationValue(
                hermite(xyzw(a.value), glm::dvec4(a.outTangent), xyzw(b.value), glm::dvec4(b.inTangent), u, dt));
        }
        const auto& values = std::get<std::vector<GtsAnimationCubicVec3Key>>(track.values);
        const auto& a      = values[left];
        const auto& b      = values[right];
        return vectorValue(hermite(
            glm::dvec3(a.value), glm::dvec3(a.outTangent), glm::dvec3(b.value), glm::dvec3(b.inTangent), u, dt));
    }
} // namespace gts::animation::detail
