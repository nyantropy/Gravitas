#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <array>
#include <stdexcept>

#include "VulkanLogicalDevice.hpp"

class GTSDescriptorSetManager 
{
    public:
        GTSDescriptorSetManager(VulkanLogicalDevice* vlogicaldevice);
        ~GTSDescriptorSetManager();

        void createDescriptorSetLayout();
        void allocateDescriptorSets();
        void updateDescriptorSets();

        VkDescriptorSetLayout& getDescriptorSetLayout();
        std::vector<VkDescriptorSet>& getDescriptorSets() const;

    private:
        VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
        std::vector<VkDescriptorSet> descriptorSets;

        VulkanLogicalDevice* vlogicaldevice;
};