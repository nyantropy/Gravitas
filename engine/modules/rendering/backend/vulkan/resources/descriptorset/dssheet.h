#pragma once

#include "DescriptorSetManager.hpp"

// Backend-private transition access for descriptor allocation.
// Keep new usage localized to Vulkan backend internals. New resource/render
// code should prefer explicit DescriptorSetManager dependencies so this sheet
// can be retired incrementally.
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

    inline const std::array<VkDescriptorSetLayout, 3>& getDescriptorSetLayouts()
    {
        return glbDSManager->getDescriptorSetLayouts();
    }
}
