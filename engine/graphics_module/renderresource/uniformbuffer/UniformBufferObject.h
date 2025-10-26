#pragma once

#include <glm.hpp>

// the uniform buffer object that will be uploaded to the shader
struct UniformBufferObject 
{
    alignas(16) glm::mat4 model;
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 proj;
};