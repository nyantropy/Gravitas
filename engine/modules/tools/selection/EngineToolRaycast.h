#pragma once

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>

#include "ActiveCameraViewStateComponent.h"
#include "BoundsComponent.h"
#include "GlmConfig.h"
#include "TransformComponent.h"

namespace gts::tools
{
    struct EngineToolPickRay
    {
        glm::vec3 origin{0.0f};
        glm::vec3 direction{0.0f, 0.0f, -1.0f};
    };

    inline std::optional<EngineToolPickRay> buildToolPickRay(const ActiveCameraViewStateComponent& camera,
                                                             float pointerX,
                                                             float pointerY)
    {
        const glm::mat4 invViewProj = glm::inverse(camera.viewProjMatrix);
        const float ndcX = pointerX * 2.0f - 1.0f;
        const float ndcY = pointerY * 2.0f - 1.0f;

        glm::vec4 nearPoint = invViewProj * glm::vec4(ndcX, ndcY, 0.0f, 1.0f);
        glm::vec4 farPoint = invViewProj * glm::vec4(ndcX, ndcY, 1.0f, 1.0f);
        if (std::abs(nearPoint.w) <= 0.00001f || std::abs(farPoint.w) <= 0.00001f)
            return std::nullopt;

        nearPoint /= nearPoint.w;
        farPoint /= farPoint.w;

        const glm::vec3 direction = glm::vec3(farPoint - nearPoint);
        const float length = glm::length(direction);
        if (length <= 0.00001f)
            return std::nullopt;

        return EngineToolPickRay{glm::vec3(nearPoint), direction / length};
    }

    inline bool slab(float origin,
                     float direction,
                     float minValue,
                     float maxValue,
                     float& tMin,
                     float& tMax)
    {
        if (std::abs(direction) <= 0.00001f)
            return origin >= minValue && origin <= maxValue;

        float t1 = (minValue - origin) / direction;
        float t2 = (maxValue - origin) / direction;
        if (t1 > t2)
            std::swap(t1, t2);

        tMin = std::max(tMin, t1);
        tMax = std::min(tMax, t2);
        return tMin <= tMax;
    }

    inline std::optional<float> intersectLocalBounds(const EngineToolPickRay& worldRay,
                                                     const TransformComponent& transform,
                                                     const BoundsComponent& bounds)
    {
        const glm::mat4 inverseModel = glm::inverse(transform.getModelMatrix());
        const glm::vec3 localOrigin = glm::vec3(inverseModel * glm::vec4(worldRay.origin, 1.0f));
        const glm::vec3 localDirection = glm::vec3(inverseModel * glm::vec4(worldRay.direction, 0.0f));

        float tMin = 0.0f;
        float tMax = std::numeric_limits<float>::max();

        if (!slab(localOrigin.x, localDirection.x, bounds.min.x, bounds.max.x, tMin, tMax)) return std::nullopt;
        if (!slab(localOrigin.y, localDirection.y, bounds.min.y, bounds.max.y, tMin, tMax)) return std::nullopt;
        if (!slab(localOrigin.z, localDirection.z, bounds.min.z, bounds.max.z, tMin, tMax)) return std::nullopt;

        return tMin;
    }

    inline float distanceRayToSegment(const EngineToolPickRay& ray,
                                      const glm::vec3& segmentStart,
                                      const glm::vec3& segmentEnd,
                                      float* outRayT = nullptr,
                                      float* outSegmentT = nullptr)
    {
        const glm::vec3 u = ray.direction;
        const glm::vec3 v = segmentEnd - segmentStart;
        const glm::vec3 w = ray.origin - segmentStart;
        const float a = glm::dot(u, u);
        const float b = glm::dot(u, v);
        const float c = glm::dot(v, v);
        const float d = glm::dot(u, w);
        const float e = glm::dot(v, w);
        const float denominator = a * c - b * b;

        float rayT = 0.0f;
        float segmentT = 0.0f;
        if (denominator > 0.00001f)
            rayT = std::max(0.0f, (b * e - c * d) / denominator);

        if (c > 0.00001f)
            segmentT = std::clamp((b * rayT + e) / c, 0.0f, 1.0f);

        if (denominator > 0.00001f)
            rayT = std::max(0.0f, (b * segmentT - d) / a);

        const glm::vec3 closestRay = ray.origin + u * rayT;
        const glm::vec3 closestSegment = segmentStart + v * segmentT;
        if (outRayT != nullptr)
            *outRayT = rayT;
        if (outSegmentT != nullptr)
            *outSegmentT = segmentT;
        return glm::length(closestRay - closestSegment);
    }

    inline std::optional<glm::vec3> intersectRayPlane(const EngineToolPickRay& ray,
                                                      const glm::vec3& planePoint,
                                                      const glm::vec3& planeNormal)
    {
        const float denominator = glm::dot(ray.direction, planeNormal);
        if (std::abs(denominator) <= 0.00001f)
            return std::nullopt;

        const float t = glm::dot(planePoint - ray.origin, planeNormal) / denominator;
        if (t < 0.0f)
            return std::nullopt;

        return ray.origin + ray.direction * t;
    }
}
