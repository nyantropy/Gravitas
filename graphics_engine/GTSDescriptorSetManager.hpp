#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <array>
#include <stdexcept>

#include "VulkanLogicalDevice.hpp"

class GTSDescriptorSetManager 
{
    public:
        GTSDescriptorSetManager(VulkanLogicalDevice* vlogicaldevice, int frames_in_flight);
        ~GTSDescriptorSetManager();

        void createDescriptorSetLayout();
        void createDescriptorPool();
        void createDescriptorSets();


        void allocateDescriptorSets();
        void updateDescriptorSets();

        VkDescriptorSetLayout& getDescriptorSetLayout();
        VkDescriptorPool& getDescriptorPool();
        std::vector<VkDescriptorSet>& getDescriptorSets() const;

    private:
        VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
        VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
        std::vector<VkDescriptorSet> descriptorSets;

        VulkanLogicalDevice* vlogicaldevice;
        int frames_in_flight;
};