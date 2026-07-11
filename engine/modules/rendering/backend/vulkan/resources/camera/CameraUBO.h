#pragma once

#include "GlmConfig.h"
#include "LightingFrameData.h"

struct GpuDirectionalLightData
{
    alignas(16) glm::vec4 directionIntensity;
    alignas(16) glm::vec4 color;
};

struct GpuPointLightData
{
    alignas(16) glm::vec4 positionRange;
    alignas(16) glm::vec4 colorIntensity;
};

struct GpuSpotLightData
{
    alignas(16) glm::vec4 positionRange;
    alignas(16) glm::vec4 directionIntensity;
    alignas(16) glm::vec4 colorInnerCone;
    alignas(16) glm::vec4 outerCone;
};

struct CameraUBO
{
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 proj;
    alignas(16) glm::vec4 cameraPosition;
    // x/y/z = directional/point/spot counts, w = ambient intensity.
    alignas(16) glm::vec4 lightingCountsAmbient;
    alignas(16) GpuDirectionalLightData directional[gts::rendering::MaxDirectionalLights];
    alignas(16) GpuPointLightData point[gts::rendering::MaxPointLights];
    alignas(16) GpuSpotLightData spot[gts::rendering::MaxSpotLights];
};
