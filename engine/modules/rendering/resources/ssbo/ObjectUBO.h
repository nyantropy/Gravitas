#pragma once

#include <glm.hpp>

struct ObjectUBO
{
    alignas(16) glm::mat4 model;
};
