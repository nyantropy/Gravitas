#pragma once

#include <string>

// Scene-authored environment lighting descriptor.
// Rendering owns texture realization, fallback resources, and GPU bindings.
struct EnvironmentLightComponent
{
    std::string environmentPath;
    float intensity = 1.0f;
    float rotationRadians = 0.0f;
    bool enabled = true;
    int priority = 0;
};
