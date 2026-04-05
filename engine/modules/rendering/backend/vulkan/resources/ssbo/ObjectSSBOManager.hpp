#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <array>
#include <cassert>
#include <stdexcept>
#include <cstring>

#include "vcsheet.h"
#include "dssheet.h"
#include "BufferUtil.hpp"
#include "ObjectUBO.h"
#include "GraphicsConstants.h"
#include "EngineConfig.h"
#include "Types.h"

// Manages a single large SSBO holding all per-object data.
// Objects are addressed by a slot index allocated via a free-list.
// Descriptor sets and GPU buffers are created once at startup and
// never scale with object count.
class ObjectSSBOManager
{
    public:
        static constexpr uint32_t MAX_OBJECTS = EngineConfig::MAX_RENDERABLE_OBJECTS;

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
                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
                    ssboBuffers[i],
                    ssboMemory[i]
                );

                vkMapMemory(vcsheet::getDevice(), ssboMemory[i], 0, bufferSize, 0, &ssboMapped[i]);

                // zero-initialize so unwritten slots don't contain garbage
                std::memset(ssboMapped[i], 0, static_cast<size_t>(bufferSize));
                lastWrittenMatrix[i].resize(MAX_OBJECTS, glm::mat4(0.0f));
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
        ssbo_id_type requestSlot()
        {
            if (!freeList.empty())
            {
                ssbo_id_type slot = freeList.back();
                freeList.pop_back();
                return slot;
            }

            assert(nextSlot < MAX_OBJECTS && "ObjectSSBOManager: MAX_OBJECTS exceeded");
            return nextSlot++;
        }

        // Returns a slot to the free-list for future reuse.
        void releaseSlot(ssbo_id_type slot)
        {
            ObjectUBO empty{};
            empty.model = glm::mat4(0.0f);
            for (uint32_t f = 0; f < static_cast<uint32_t>(ssboMapped.size()); ++f)
            {
                ObjectUBO* base = reinterpret_cast<ObjectUBO*>(ssboMapped[f]);
                base[slot] = empty;
            }

            freeList.push_back(slot);
            for (auto& frameCache : lastWrittenMatrix)
                frameCache[slot] = glm::mat4(0.0f);
        }

        // Writes one object's data into the given frame's SSBO at the given slot.
        bool writeSlot(uint32_t frameIndex, ssbo_id_type slot, const ObjectUBO& data)
        {
            if (lastWrittenMatrix[frameIndex][slot] == data.model)
                return false;

            lastWrittenMatrix[frameIndex][slot] = data.model;
            ObjectUBO* base = reinterpret_cast<ObjectUBO*>(ssboMapped[frameIndex]);
            base[slot] = data;
            return true;
        }

        void flushFrame(uint32_t frameIndex)
        {
            VkMappedMemoryRange range{};
            range.sType  = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
            range.memory = ssboMemory[frameIndex];
            range.offset = 0;
            range.size   = VK_WHOLE_SIZE;
            vkFlushMappedMemoryRanges(vcsheet::getDevice(), 1, &range);
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
        std::array<std::vector<glm::mat4>, GraphicsConstants::MAX_FRAMES_IN_FLIGHT> lastWrittenMatrix;

        std::vector<ssbo_id_type> freeList;
        ssbo_id_type              nextSlot = 0;
};
