#pragma once

#include <vulkan/vulkan.h>
#include <array>
#include <cassert>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <vector>

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
            VkPhysicalDeviceProperties props{};
            vkGetPhysicalDeviceProperties(vcsheet::getPhysicalDevice(), &props);
            nonCoherentAtomSize = props.limits.nonCoherentAtomSize;

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

            for (uint32_t i = 0; i < GraphicsConstants::MAX_FRAMES_IN_FLIGHT; ++i)
            {
                m_dirtyMin[i] = std::numeric_limits<uint32_t>::max();
                m_dirtyMax[i] = 0;
            }
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
                flushSlotRange(f, slot);
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

            const uint32_t slotU32 = static_cast<uint32_t>(slot);
            m_dirtyMin[frameIndex] = std::min(m_dirtyMin[frameIndex], slotU32);
            m_dirtyMax[frameIndex] = std::max(m_dirtyMax[frameIndex], slotU32);

            return true;
        }

        uint32_t writeSlotAllFrames(ssbo_id_type slot, const ObjectUBO& data)
        {
            uint32_t writes = 0;
            for (uint32_t frameIndex = 0; frameIndex < static_cast<uint32_t>(ssboMapped.size()); ++frameIndex)
            {
                if (writeSlot(frameIndex, slot, data))
                    writes += 1;
            }

            return writes;
        }

        void flushFrame(uint32_t frameIndex)
        {
            // Dirty-range tracking: part of the engine's dirty-notification pattern.
            // Only the modified slot range is flushed to GPU each frame.
            // See also: sortOrderDirty in RenderCommandExtractor (sort path).
            if (m_dirtyMin[frameIndex] > m_dirtyMax[frameIndex])
                return;

            const VkDeviceSize slotSize  = sizeof(ObjectUBO);
            const VkDeviceSize totalSize = static_cast<VkDeviceSize>(MAX_OBJECTS) * slotSize;
            const VkDeviceSize atomSize  = nonCoherentAtomSize > 0 ? nonCoherentAtomSize : slotSize;

            const VkDeviceSize rawOffset = static_cast<VkDeviceSize>(m_dirtyMin[frameIndex]) * slotSize;
            const VkDeviceSize rawEnd    = static_cast<VkDeviceSize>(m_dirtyMax[frameIndex] + 1) * slotSize;

            const VkDeviceSize alignedOffset = (rawOffset / atomSize) * atomSize;
            const VkDeviceSize alignedEnd    = std::min(((rawEnd + atomSize - 1) / atomSize) * atomSize, totalSize);

            VkMappedMemoryRange range{};
            range.sType  = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
            range.memory = ssboMemory[frameIndex];
            range.offset = alignedOffset;
            range.size   = alignedEnd - alignedOffset;
            vkFlushMappedMemoryRanges(vcsheet::getDevice(), 1, &range);

            m_dirtyMin[frameIndex] = std::numeric_limits<uint32_t>::max();
            m_dirtyMax[frameIndex] = 0;
        }

        void flushAllFrames()
        {
            for (uint32_t frameIndex = 0; frameIndex < static_cast<uint32_t>(ssboMemory.size()); ++frameIndex)
                flushFrame(frameIndex);
        }

        // Returns the descriptor set for the given frame index (bound to set 1).
        VkDescriptorSet getDescriptorSet(uint32_t frameIndex) const
        {
            return descriptorSets[frameIndex];
        }

    private:
        void flushSlotRange(uint32_t frameIndex, ssbo_id_type slot)
        {
            const VkDeviceSize slotOffset = static_cast<VkDeviceSize>(slot) * sizeof(ObjectUBO);
            const VkDeviceSize atomSize = nonCoherentAtomSize > 0 ? nonCoherentAtomSize : sizeof(ObjectUBO);
            const VkDeviceSize alignedOffset = (slotOffset / atomSize) * atomSize;
            const VkDeviceSize slotEnd = slotOffset + sizeof(ObjectUBO);
            const VkDeviceSize alignedEnd = ((slotEnd + atomSize - 1) / atomSize) * atomSize;

            VkMappedMemoryRange range{};
            range.sType  = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
            range.memory = ssboMemory[frameIndex];
            range.offset = alignedOffset;
            range.size   = alignedEnd - alignedOffset;
            vkFlushMappedMemoryRanges(vcsheet::getDevice(), 1, &range);
        }

        std::vector<VkBuffer>        ssboBuffers;
        std::vector<VkDeviceMemory>  ssboMemory;
        std::vector<void*>           ssboMapped;
        std::vector<VkDescriptorSet> descriptorSets;
        std::array<std::vector<glm::mat4>, GraphicsConstants::MAX_FRAMES_IN_FLIGHT> lastWrittenMatrix;
        VkDeviceSize                 nonCoherentAtomSize = sizeof(ObjectUBO);

        // Dirty-range tracking — initialised to "empty" (min > max).
        // Updated by writeSlot; consumed and reset by flushFrame.
        std::array<uint32_t, GraphicsConstants::MAX_FRAMES_IN_FLIGHT> m_dirtyMin;
        std::array<uint32_t, GraphicsConstants::MAX_FRAMES_IN_FLIGHT> m_dirtyMax;

        std::vector<ssbo_id_type> freeList;
        ssbo_id_type              nextSlot = 0;
};
