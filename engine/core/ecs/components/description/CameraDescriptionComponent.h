#pragma once

#include <glm.hpp>
#include <gtc/matrix_transform.hpp>

// Gameplay-facing camera description.  Contains only camera intent —
// no GPU handles, no pre-computed matrices.
// CameraGpuSystem reads this and drives CameraGpuComponent.
struct CameraDescriptionComponent
{
    glm::vec3 target      = glm::vec3(0.0f, 0.0f, 0.0f);
    glm::vec3 up          = glm::vec3(0.0f, 1.0f, 0.0f);
    float fov             = glm::radians(60.0f);
    float nearClip        = 0.1f;
    float farClip         = 1000.0f;
    float aspectRatio     = 800.0f / 800.0f;
    bool  active          = false;
};
