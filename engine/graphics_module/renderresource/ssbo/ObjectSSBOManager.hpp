#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <cassert>
#include <stdexcept>
#include <cstring>

#include "vcsheet.h"
#include "dssheet.h"
#include "BufferUtil.hpp"
#include "ObjectUBO.h"
#include "GraphicsConstants.h"

// Manages a single large SSBO holding all per-object data.
// Objects are addressed by a slot index allocated via a free-list.
// Descriptor sets and GPU buffers are created once at startup and
// never scale with object count.
class ObjectSSBOManager
{
    public:
        static constexpr uint32_t MAX_OBJECTS = 4096;

        ObjectSSBOManager()
        {
            VkDeviceSize bufferSize = static_cast<VkDeviceSize>(MAX_OBJECTS) * sizeof(ObjectUBO);
            uint32_t frames = static_cast<uint32_t>(GraphicsConstants::MAX_FRAMES_IN_FLIGHT);

            ssboBuffers.resize(frames);
            ssboMemory.resize(frames);
            ssboMapped.resize(frames);

            for (uint32_t i = 0; i < frames; ++i)
            {
                BufferUtil::createBuffer(
                    vcsheet::getDevice(),
                    vcsheet::getPhysicalDevice(),
                    bufferSize,
                    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                    ssboBuffers[i],
                    ssboMemory[i]
                );

                vkMapMemory(vcsheet::getDevice(), ssboMemory[i], 0, bufferSize, 0, &ssboMapped[i]);

                // zero-initialize so unwritten slots don't contain garbage
                std::memset(ssboMapped[i], 0, static_cast<size_t>(bufferSize));
            }

            descriptorSets = dssheet::getManager().allocateForObjectSSBO(ssboBuffers, bufferSize);
        }

        ~ObjectSSBOManager()
        {
            for (size_t i = 0; i < ssboBuffers.size(); ++i)
            {
                if (ssboMapped[i])
                    vkUnmapMemory(vcsheet::getDevice(), ssboMemory[i]);
                if (ssboBuffers[i] != VK_NULL_HANDLE)
                    vkDestroyBuffer(vcsheet::getDevice(), ssboBuffers[i], nullptr);
                if (ssboMemory[i] != VK_NULL_HANDLE)
                    vkFreeMemory(vcsheet::getDevice(), ssboMemory[i], nullptr);
            }
        }

        // Returns a free slot index for a new object.
        uint32_t requestSlot()
        {
            if (!freeList.empty())
            {
                uint32_t slot = freeList.back();
                freeList.pop_back();
                return slot;
            }

            assert(nextSlot < MAX_OBJECTS && "ObjectSSBOManager: MAX_OBJECTS exceeded");
            return nextSlot++;
        }

        // Returns a slot to the free-list for future reuse.
        void releaseSlot(uint32_t slot)
        {
            freeList.push_back(slot);
        }

        // Writes one object's data into the given frame's SSBO at the given slot.
        void writeSlot(uint32_t frameIndex, uint32_t slot, const ObjectUBO& data)
        {
            ObjectUBO* base = reinterpret_cast<ObjectUBO*>(ssboMapped[frameIndex]);
            base[slot] = data;
        }

        // Returns the descriptor set for the given frame index (bound to set 1).
        VkDescriptorSet getDescriptorSet(uint32_t frameIndex) const
        {
            return descriptorSets[frameIndex];
        }

    private:
        std::vector<VkBuffer>        ssboBuffers;
        std::vector<VkDeviceMemory>  ssboMemory;
        std::vector<void*>           ssboMapped;
        std::vector<VkDescriptorSet> descriptorSets;

        std::vector<uint32_t> freeList;
        uint32_t              nextSlot = 0;
};
