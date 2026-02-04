#pragma once

#include <glm.hpp>
#include <gtc/matrix_transform.hpp>

struct CameraComponent
{
    glm::mat4 view = glm::mat4(1.0f);
    glm::mat4 projection = glm::mat4(1.0f);

    // get position from transform component instead
    //glm::vec3 position = glm::vec3(0.0f, 0.0f, 10.0f);
    glm::vec3 target = glm::vec3(0.0f, 0.0f, 0.0f);
    glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);

    float fov = glm::radians(60.0f);
    float nearClip = 0.1f;
    float farClip = 1000.0f;
    float aspectRatio = 800.0f / 800.0f;
};