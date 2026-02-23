#pragma once

#include <glm.hpp>

struct CameraUBO
{
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 proj;
};
