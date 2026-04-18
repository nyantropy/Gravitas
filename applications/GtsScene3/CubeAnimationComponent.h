#pragma once

#include "GlmConfig.h"

struct CubeAnimationComponent
{
    int       animationType   = 0;
    glm::vec3 homePosition    = glm::vec3(0.0f);
    float     phase           = 0.0f;
    float     speed           = 1.0f;
    glm::vec3 axis            = glm::vec3(0.0f, 1.0f, 0.0f);
    float     accumulatedTime = 0.0f;
    float     amplitude       = 0.5f;
    float     orbitRadius     = 1.0f;
    float     rotationAmount  = 0.5f;
};
