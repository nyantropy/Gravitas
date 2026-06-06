#pragma once

#include <array>
#include <algorithm>
#include <cmath>

#include "ActiveCameraViewStateComponent.h"
#include "BoundsComponent.h"
#include "DebugDrawQueueComponent.h"
#include "DebugDrawSettingsComponent.h"
#include "ECSWorld.hpp"
#include "GlmConfig.h"
#include "TransformComponent.h"

namespace gts::debugdraw
{
    inline DebugDrawQueueComponent& ensureQueue(ECSWorld& world)
    {
        if (!world.hasAny<DebugDrawQueueComponent>())
            return world.createSingleton<DebugDrawQueueComponent>();
        return world.getSingleton<DebugDrawQueueComponent>();
    }

    inline DebugDrawSettingsComponent& ensureSettings(ECSWorld& world)
    {
        if (!world.hasAny<DebugDrawSettingsComponent>())
            return world.createSingleton<DebugDrawSettingsComponent>();
        return world.getSingleton<DebugDrawSettingsComponent>();
    }

    inline void clear(ECSWorld& world)
    {
        DebugDrawQueueComponent& queue = ensureQueue(world);
        queue.clear();
    }

    inline void line(DebugDrawQueueComponent& queue,
                     const glm::vec3& start,
                     const glm::vec3& end,
                     DebugDrawColor color,
                     float thickness)
    {
        const glm::vec3 delta = end - start;
        if (glm::dot(delta, delta) <= 0.00000001f)
            return;

        queue.pushLine({start, end, color, std::max(0.001f, thickness)});
    }

    inline void line(ECSWorld& world,
                     const glm::vec3& start,
                     const glm::vec3& end,
                     DebugDrawColor color,
                     float thickness)
    {
        DebugDrawQueueComponent& queue = ensureQueue(world);
        line(queue, start, end, color, thickness);
    }

    inline glm::vec3 transformPoint(const glm::mat4& matrix, const glm::vec3& point)
    {
        return glm::vec3(matrix * glm::vec4(point, 1.0f));
    }

    inline std::array<glm::vec3, 8> boundsCorners(const BoundsComponent& bounds,
                                                  const glm::mat4& model)
    {
        const glm::vec3 min = bounds.min;
        const glm::vec3 max = bounds.max;
        return {
            transformPoint(model, {min.x, min.y, min.z}),
            transformPoint(model, {max.x, min.y, min.z}),
            transformPoint(model, {max.x, max.y, min.z}),
            transformPoint(model, {min.x, max.y, min.z}),
            transformPoint(model, {min.x, min.y, max.z}),
            transformPoint(model, {max.x, min.y, max.z}),
            transformPoint(model, {max.x, max.y, max.z}),
            transformPoint(model, {min.x, max.y, max.z})
        };
    }

    inline size_t buildBoundsLines(const TransformComponent& transform,
                                   const BoundsComponent& localBounds,
                                   DebugDrawColor color,
                                   float thickness,
                                   std::array<DebugDrawLine, 12>& outLines)
    {
        const auto corners = boundsCorners(localBounds, transform.getModelMatrix());
        constexpr std::array<std::array<int, 2>, 12> edges{{
            {{0, 1}}, {{1, 2}}, {{2, 3}}, {{3, 0}},
            {{4, 5}}, {{5, 6}}, {{6, 7}}, {{7, 4}},
            {{0, 4}}, {{1, 5}}, {{2, 6}}, {{3, 7}}
        }};

        size_t count = 0;
        const float safeThickness = std::max(0.001f, thickness);
        for (const auto& edge : edges)
        {
            const glm::vec3& start = corners[static_cast<size_t>(edge[0])];
            const glm::vec3& end = corners[static_cast<size_t>(edge[1])];
            const glm::vec3 delta = end - start;
            if (glm::dot(delta, delta) <= 0.00000001f)
                continue;

            outLines[count++] = {start, end, color, safeThickness};
        }

        return count;
    }

    inline void bounds(DebugDrawQueueComponent& queue,
                       const TransformComponent& transform,
                       const BoundsComponent& localBounds,
                       DebugDrawColor color,
                       float thickness)
    {
        std::array<DebugDrawLine, 12> lines{};
        const size_t lineCount = buildBoundsLines(transform, localBounds, color, thickness, lines);
        queue.appendLines(color, lines.data(), lineCount);
    }

    inline void bounds(ECSWorld& world,
                       const TransformComponent& transform,
                       const BoundsComponent& localBounds,
                       DebugDrawColor color,
                       float thickness)
    {
        DebugDrawQueueComponent& queue = ensureQueue(world);
        bounds(queue, transform, localBounds, color, thickness);
    }

    inline glm::mat3 rotationBasis(const TransformComponent& transform)
    {
        glm::mat4 rotation(1.0f);
        rotation = glm::rotate(rotation, transform.rotation.x, glm::vec3(1.0f, 0.0f, 0.0f));
        rotation = glm::rotate(rotation, transform.rotation.y, glm::vec3(0.0f, 1.0f, 0.0f));
        rotation = glm::rotate(rotation, transform.rotation.z, glm::vec3(0.0f, 0.0f, 1.0f));
        return glm::mat3(rotation);
    }

    inline void basis(ECSWorld& world,
                      const TransformComponent& transform,
                      float length,
                      float thickness,
                      bool localSpace)
    {
        const glm::mat3 axes = localSpace ? rotationBasis(transform) : glm::mat3(1.0f);
        const glm::vec3 origin = transform.position;
        line(world, origin, origin + glm::normalize(axes[0]) * length, DebugDrawColor::Red, thickness);
        line(world, origin, origin + glm::normalize(axes[1]) * length, DebugDrawColor::Green, thickness);
        line(world, origin, origin + glm::normalize(axes[2]) * length, DebugDrawColor::Blue, thickness);
    }

    inline void ray(ECSWorld& world,
                    const glm::vec3& origin,
                    const glm::vec3& direction,
                    float length,
                    DebugDrawColor color,
                    float thickness)
    {
        const float dirLength = glm::length(direction);
        if (dirLength <= 0.0001f)
            return;

        line(world, origin, origin + direction / dirLength * length, color, thickness);
    }

    inline void frustumFromViewProj(ECSWorld& world,
                                    const glm::mat4& viewProjMatrix,
                                    DebugDrawColor color,
                                    float thickness)
    {
        const glm::mat4 inverseViewProj = glm::inverse(viewProjMatrix);
        std::array<glm::vec3, 8> corners{};
        size_t index = 0;
        for (float z : {0.0f, 1.0f})
        {
            for (float y : {-1.0f, 1.0f})
            {
                for (float x : {-1.0f, 1.0f})
                {
                    glm::vec4 point = inverseViewProj * glm::vec4(x, y, z, 1.0f);
                    if (std::abs(point.w) <= 0.00001f)
                        return;
                    corners[index++] = glm::vec3(point / point.w);
                }
            }
        }

        constexpr std::array<std::array<int, 2>, 12> edges{{
            {{0, 1}}, {{1, 3}}, {{3, 2}}, {{2, 0}},
            {{4, 5}}, {{5, 7}}, {{7, 6}}, {{6, 4}},
            {{0, 4}}, {{1, 5}}, {{2, 6}}, {{3, 7}}
        }};
        for (const auto& edge : edges)
            line(world, corners[static_cast<size_t>(edge[0])], corners[static_cast<size_t>(edge[1])], color, thickness);
    }

    inline void frustum(ECSWorld& world,
                        const ActiveCameraViewStateComponent& camera,
                        DebugDrawColor color,
                        float thickness)
    {
        if (!camera.valid)
            return;

        frustumFromViewProj(world, camera.viewProjMatrix, color, thickness);
    }
}
