#pragma once

#include <vector>
#include <vulkan/vulkan.h>

// GPU resources for one render view's camera data.
// One CameraBufferResource exists per render view (not per object, not per entity).
// Holds framesInFlight UBOs and their corresponding descriptor sets.
struct CameraBufferResource
{
    std::vector<VkBuffer>        uniformBuffers;
    std::vector<VkDeviceMemory>  uniformBuffersMemory;
    std::vector<void*>           uniformBuffersMapped;
    std::vector<VkDescriptorSet> descriptorSets;
};
