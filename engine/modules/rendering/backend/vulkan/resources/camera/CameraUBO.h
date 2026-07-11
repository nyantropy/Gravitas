#pragma once

#include "GlmConfig.h"

struct CameraUBO
{
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 proj;
    alignas(16) glm::vec4 cameraPosition;
    alignas(16) glm::vec4 lightDirectionIntensity;
    alignas(16) glm::vec4 lightColorAmbient;
};
