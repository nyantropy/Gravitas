#pragma once

#include <algorithm>
#include <cmath>

#include "GlmConfig.h"

#include "EngineToolInputCaptureComponent.h"

namespace gts::tools
{
    struct EngineToolOrbitCameraState
    {
        bool initialized = false;

        glm::vec3 target = {0.0f, 0.0f, 0.0f};
        float yaw = 0.0f;
        float pitch = glm::radians(18.0f);
        float distance = 4.0f;
        float minDistance = 0.25f;
        float maxDistance = 80.0f;
    };

    inline bool viewportAcceptsCameraInput(const EngineToolInputCaptureComponent& capture)
    {
        return capture.pointerOverViewport && !capture.pointerOverToolUi && !capture.toolUiPressed;
    }

    inline void resetOrbitCamera(EngineToolOrbitCameraState& state,
                                 glm::vec3 position,
                                 glm::vec3 target,
                                 float minDistance,
                                 float maxDistance)
    {
        const glm::vec3 offset = position - target;
        const float distance = std::max(0.001f, glm::length(offset));
        const glm::vec3 direction = offset / distance;

        state.initialized = true;
        state.target = target;
        state.yaw = std::atan2(direction.x, direction.z);
        state.pitch = glm::clamp(std::asin(glm::clamp(direction.y, -1.0f, 1.0f)),
                                 glm::radians(-84.0f),
                                 glm::radians(84.0f));
        state.minDistance = std::max(0.01f, minDistance);
        state.maxDistance = std::max(state.minDistance + 0.01f, maxDistance);
        state.distance = glm::clamp(distance, state.minDistance, state.maxDistance);
    }

    inline glm::vec3 orbitCameraPosition(const EngineToolOrbitCameraState& state)
    {
        const float cosPitch = std::cos(state.pitch);
        const glm::vec3 offset{
            std::sin(state.yaw) * cosPitch * state.distance,
            std::sin(state.pitch) * state.distance,
            std::cos(state.yaw) * cosPitch * state.distance
        };
        return state.target + offset;
    }

    inline bool applyOrbitCameraInput(EngineToolOrbitCameraState& state,
                                      const EngineToolInputCaptureComponent& capture,
                                      bool primaryDragOrbits)
    {
        if (!state.initialized)
            return false;

        const bool acceptsInput = viewportAcceptsCameraInput(capture);
        const bool orbiting = acceptsInput && (capture.orbitDown || (primaryDragOrbits && capture.primaryDown));
        const bool panning = acceptsInput && capture.panDown;
        const bool scrolling = acceptsInput && std::fabs(capture.scrollY) > 0.0001f;
        bool changed = false;

        if (orbiting)
        {
            state.yaw -= capture.pointerDeltaX * 0.0060f;
            state.pitch -= capture.pointerDeltaY * 0.0060f;
            state.pitch = glm::clamp(state.pitch, glm::radians(-84.0f), glm::radians(84.0f));
            changed = true;
        }

        if (panning)
        {
            const glm::vec3 position = orbitCameraPosition(state);
            const glm::vec3 forward = glm::normalize(state.target - position);
            glm::vec3 right = glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f));
            if (glm::dot(right, right) <= 0.000001f)
                right = {1.0f, 0.0f, 0.0f};
            else
                right = glm::normalize(right);
            const glm::vec3 up = glm::normalize(glm::cross(right, forward));
            const float panScale = std::max(0.001f, state.distance) * 0.0016f;
            state.target += (-right * capture.pointerDeltaX + up * capture.pointerDeltaY) * panScale;
            changed = true;
        }

        if (scrolling)
        {
            const float zoomFactor = std::pow(0.88f, capture.scrollY);
            state.distance = glm::clamp(state.distance * zoomFactor,
                                        state.minDistance,
                                        state.maxDistance);
            changed = true;
        }

        return changed;
    }
} // namespace gts::tools
