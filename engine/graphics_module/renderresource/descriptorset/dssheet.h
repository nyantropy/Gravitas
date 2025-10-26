#pragma once

#include "DescriptorSetManager.hpp"

// its easier for the descriptorsetmanager to have a global access sheet similar to the vulkan context
// since it makes a lot of sense to couple them with the texture and the uniform buffers
namespace dssheet
{
    inline DescriptorSetManager* glbDSManager = nullptr;

    inline void SetManager(DescriptorSetManager* mgr)
    {
        glbDSManager = mgr;
    }

    inline DescriptorSetManager& getManager()
    {
        return *glbDSManager;
    }

    inline const std::array<VkDescriptorSetLayout, 2>& getDescriptorSetLayouts()
    {
        return glbDSManager->getDescriptorSetLayouts();
    }
}
