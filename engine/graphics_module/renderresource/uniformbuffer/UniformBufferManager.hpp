#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <unordered_map>
#include <memory>
#include <stdexcept>

#include "vcsheet.h"
#include "dssheet.h"
#include "BufferUtil.hpp"
#include "UniformBufferResource.h"
#include "GraphicsConstants.h"
#include "Types.h"

// manages uniform buffer resources and descriptor sets per entity or object.
class UniformBufferManager
{
    private:
        std::unordered_map<uniform_id_type, std::unique_ptr<UniformBufferResource>> idToUBO;
        uniform_id_type nextID = 1; // 0 = invalid

    public:
        UniformBufferManager() = default;

        ~UniformBufferManager()
        {
            for (auto& [id, ubo] : idToUBO)
            {
                for (size_t i = 0; i < ubo->uniformBuffers.size(); i++)
                {
                    if (ubo->uniformBuffers[i] != VK_NULL_HANDLE)
                        vkDestroyBuffer(vcsheet::getDevice(), ubo->uniformBuffers[i], nullptr);
                    if (ubo->uniformBuffersMemory[i] != VK_NULL_HANDLE)
                        vkFreeMemory(vcsheet::getDevice(), ubo->uniformBuffersMemory[i], nullptr);
                }
            }

            idToUBO.clear();
        }

        // creates a new uniform buffer resource and returns its unique ID
        uniform_id_type createUniformBuffer()
        {
            auto ubo = std::make_unique<UniformBufferResource>();

            BufferUtil::createUniformBuffers(
                vcsheet::getDevice(),
                vcsheet::getPhysicalDevice(),
                ubo->uniformBuffers,
                ubo->uniformBuffersMemory,
                ubo->uniformBuffersMapped,
                GraphicsConstants::MAX_FRAMES_IN_FLIGHT
            );

            ubo->descriptorSets = dssheet::getManager()
                .allocateForUniformBuffer(ubo->uniformBuffers, sizeof(UniformBufferObject));

            uniform_id_type id = nextID++;
            idToUBO[id] = std::move(ubo);

            return id;
        }

        UniformBufferResource* getUniformBuffer(uniform_id_type id)
        {
            auto it = idToUBO.find(id);
            if (it != idToUBO.end())
                return it->second.get();
            return nullptr;
        }
};
