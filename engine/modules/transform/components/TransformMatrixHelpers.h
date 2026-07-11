#pragma once

#include "GlmConfig.h"
#include "TransformComponent.h"

#include <gtc/quaternion.hpp>
#include <gtx/matrix_decompose.hpp>
#include <gtx/quaternion.hpp>

namespace gts::transform
{
    inline glm::vec3 worldPositionFromMatrix(const glm::mat4& matrix)
    {
        return glm::vec3(matrix[3]);
    }

    inline bool decomposeMatrixToTransform(const glm::mat4& matrix, TransformComponent& outTransform)
    {
        glm::vec3 skew;
        glm::vec4 perspective;
        glm::quat orientation;
        glm::vec3 translation;
        glm::vec3 scale;

        if (!glm::decompose(matrix, scale, orientation, translation, skew, perspective))
            return false;

        outTransform.position = translation;
        outTransform.rotation = glm::eulerAngles(orientation);
        outTransform.scale = scale;
        return true;
    }

    inline TransformComponent transformFromMatrix(const glm::mat4& matrix)
    {
        TransformComponent transform;
        decomposeMatrixToTransform(matrix, transform);
        return transform;
    }
}
