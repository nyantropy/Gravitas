#pragma once

#include <glm.hpp>
#include <gtc/matrix_transform.hpp>
#include <chrono>

class GtsAnimation
{
    public:
        glm::mat4 getModelMatrix(float deltaTime)
        {
            return glm::rotate(glm::mat4(1.0f), deltaTime * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
        }
};