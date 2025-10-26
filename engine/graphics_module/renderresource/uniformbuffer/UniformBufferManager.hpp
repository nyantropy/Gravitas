#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <unordered_map>
#include <memory>
#include <stdexcept>

#include "vcsheet.h"
#include "dssheet.h"
#include "BufferUtil.hpp"
#include "Entity.h"
#include "UniformBufferResource.h"
#include "GraphicsConstants.h"
#include "Types.h"

// reminder: uniform buffers are once per entity, which is why we use the entities id to save them in the manager class
class UniformBufferManager
{
    public:
        UniformBufferManager()
        {}

        ~UniformBufferManager()
        {
            cleanup();
        }

        // create a uniform buffer resource for a specific entity (based on id)
        UniformBufferResource& createUniformBufferResource(entity_id_type id)
        {
            if (uboCache.find(id) != uboCache.end())
                return *uboCache[id];

            // create a uniform buffer resource
            std::unique_ptr<UniformBufferResource> ubo = std::make_unique<UniformBufferResource>();

            // create the uniform buffers using a utility class
            BufferUtil::createUniformBuffers(vcsheet::getDevice(), vcsheet::getPhysicalDevice(),
            ubo->uniformBuffers, ubo->uniformBuffersMemory, ubo->uniformBuffersMapped,
            GraphicsConstants::MAX_FRAMES_IN_FLIGHT);

            // let the descriptor set manager allocate appropriate descriptor sets
            ubo->descriptorSets = dssheet::getManager().allocateForUniformBuffer(ubo->uniformBuffers, sizeof(UniformBufferObject));

            UniformBufferResource& ref = *ubo;
            uboCache[id] = std::move(ubo);
            return ref;
        }

        UniformBufferResource* getUniformBufferResource(entity_id_type id)
        {
            auto it = uboCache.find(id);
            if (it != uboCache.end())
                return it->second.get();
            return nullptr;
        }

        void cleanup()
        {
            for (auto& [id, ubo] : uboCache)
            {
                for (size_t i = 0; i < ubo->uniformBuffers.size(); i++)
                {
                    if (ubo->uniformBuffers[i] != VK_NULL_HANDLE)
                        vkDestroyBuffer(vcsheet::getDevice(), ubo->uniformBuffers[i], nullptr);
                    if (ubo->uniformBuffersMemory[i] != VK_NULL_HANDLE)
                        vkFreeMemory(vcsheet::getDevice(), ubo->uniformBuffersMemory[i], nullptr);
                }
            }
            uboCache.clear();
        }

    private:
        std::unordered_map<entity_id_type, std::unique_ptr<UniformBufferResource>> uboCache;
};
