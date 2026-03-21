#pragma once

#include "GlmConfig.h"

struct ObjectUBO
{
    alignas(16) glm::mat4 model;
};
