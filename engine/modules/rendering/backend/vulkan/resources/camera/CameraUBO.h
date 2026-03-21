#pragma once

#include "GlmConfig.h"

struct CameraUBO
{
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 proj;
};
