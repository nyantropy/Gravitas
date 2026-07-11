#pragma once

#include <vulkan/vulkan.h>
#include <array>
#include <cassert>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <vector>

#include "DescriptorSetManager.hpp"
#include "VulkanBackendContext.h"
#include "BufferUtil.hpp"
#include "ObjectUBO.h"
#include "GraphicsConstants.h"
#include "Types.h"

// Manages a single large SSBO holding all per-object data.
// Objects are addressed by a slot index allocated via a free-list.
// Descriptor sets and GPU buffers are created once at startup and
// never scale with object count.
class ObjectSSBOManager
{
    public:
        static constexpr uint32_t MAX_OBJECTS = GraphicsConstants::MAX_RENDERABLE_OBJECTS;

        ObjectSSBOManager(VulkanBackendContext& backendContext, DescriptorSetManager& descriptorSetManager)
            : backendContext(backendContext)
            , descriptorSetManager(descriptorSetManager)
        {
            VkDeviceSize bufferSize = static_cast<VkDeviceSize>(MAX_OBJECTS) * sizeof(ObjectUBO);
            uint32_t frames = static_cast<uint32_t>(GraphicsConstants::MAX_FRAMES_IN_FLIGHT);
            VkPhysicalDeviceProperties props{};
            vkGetPhysicalDeviceProperties(backendContext.physicalDevice(), &props);
            nonCoherentAtomSize = props.limits.nonCoherentAtomSize;

            ssboBuffers.resize(frames);
            ssboMemory.resize(frames);
            ssboMapped.resize(frames);

            for (uint32_t i = 0; i < frames; ++i)
            {
                BufferUtil::createBuffer(
                    backendContext.device(),
                    backendContext.physicalDevice(),
                    bufferSize,
                    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
                    ssboBuffers[i],
                    ssboMemory[i]
                );

                vkMapMemory(backendContext.device(), ssboMemory[i], 0, bufferSize, 0, &ssboMapped[i]);

                // zero-initialize so unwritten slots don't contain garbage
                std::memset(ssboMapped[i], 0, static_cast<size_t>(bufferSize));
                lastWrittenObject[i].resize(MAX_OBJECTS, emptyObjectData());
            }

            descriptorSets = descriptorSetManager.allocateForObjectSSBO(ssboBuffers, bufferSize);

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
                    vkUnmapMemory(backendContext.device(), ssboMemory[i]);
                if (ssboBuffers[i] != VK_NULL_HANDLE)
                    vkDestroyBuffer(backendContext.device(), ssboBuffers[i], nullptr);
                if (ssboMemory[i] != VK_NULL_HANDLE)
                    vkFreeMemory(backendContext.device(), ssboMemory[i], nullptr);
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
            ObjectUBO empty = emptyObjectData();
            for (uint32_t f = 0; f < static_cast<uint32_t>(ssboMapped.size()); ++f)
            {
                ObjectUBO* base = reinterpret_cast<ObjectUBO*>(ssboMapped[f]);
                base[slot] = empty;
                flushSlotRange(f, slot);
            }

            freeList.push_back(slot);
            for (auto& frameCache : lastWrittenObject)
                frameCache[slot] = emptyObjectData();
        }

        // Writes one object's data into the given frame's SSBO at the given slot.
        bool writeSlot(uint32_t frameIndex, ssbo_id_type slot, const ObjectUBO& data)
        {
            if (sameObjectData(lastWrittenObject[frameIndex][slot], data))
                return false;

            lastWrittenObject[frameIndex][slot] = data;
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

        bool writeSlotObjectData(uint32_t frameIndex,
                                 ssbo_id_type slot,
                                 const glm::mat4& model,
                                 const glm::vec4& uvTransform)
        {
            ObjectUBO data = lastWrittenObject[frameIndex][slot];
            data.model = model;
            data.uvTransform = uvTransform;
            return writeSlot(frameIndex, slot, data);
        }

        uint32_t writeSlotObjectDataAllFrames(ssbo_id_type slot,
                                              const glm::mat4& model,
                                              const glm::vec4& uvTransform)
        {
            uint32_t writes = 0;
            for (uint32_t frameIndex = 0; frameIndex < static_cast<uint32_t>(ssboMapped.size()); ++frameIndex)
            {
                if (writeSlotObjectData(frameIndex, slot, model, uvTransform))
                    writes += 1;
            }

            return writes;
        }

        bool writeSlotTint(uint32_t frameIndex, ssbo_id_type slot, const glm::vec4& tint)
        {
            ObjectUBO data = lastWrittenObject[frameIndex][slot];
            if (data.tint == tint)
                return false;

            data.tint = tint;
            return writeSlot(frameIndex, slot, data);
        }

        uint32_t writeSlotTintAllFrames(ssbo_id_type slot, const glm::vec4& tint)
        {
            uint32_t writes = 0;
            for (uint32_t frameIndex = 0; frameIndex < static_cast<uint32_t>(ssboMapped.size()); ++frameIndex)
            {
                if (writeSlotTint(frameIndex, slot, tint))
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
            vkFlushMappedMemoryRanges(backendContext.device(), 1, &range);

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
        VulkanBackendContext& backendContext;
        DescriptorSetManager& descriptorSetManager;

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
            vkFlushMappedMemoryRanges(backendContext.device(), 1, &range);
        }

        std::vector<VkBuffer>        ssboBuffers;
        std::vector<VkDeviceMemory>  ssboMemory;
        std::vector<void*>           ssboMapped;
        std::vector<VkDescriptorSet> descriptorSets;
        std::array<std::vector<ObjectUBO>, GraphicsConstants::MAX_FRAMES_IN_FLIGHT> lastWrittenObject;
        VkDeviceSize                 nonCoherentAtomSize = sizeof(ObjectUBO);

        // Dirty-range tracking — initialised to "empty" (min > max).
        // Updated by writeSlot; consumed and reset by flushFrame.
        std::array<uint32_t, GraphicsConstants::MAX_FRAMES_IN_FLIGHT> m_dirtyMin;
        std::array<uint32_t, GraphicsConstants::MAX_FRAMES_IN_FLIGHT> m_dirtyMax;

        std::vector<ssbo_id_type> freeList;
        ssbo_id_type              nextSlot = 0;

        static ObjectUBO emptyObjectData()
        {
            ObjectUBO empty{};
            empty.model = glm::mat4(0.0f);
            empty.uvTransform = {1.0f, 1.0f, 0.0f, 0.0f};
            empty.tint = {1.0f, 1.0f, 1.0f, 1.0f};
            return empty;
        }

        static bool sameObjectData(const ObjectUBO& lhs, const ObjectUBO& rhs)
        {
            return lhs.model == rhs.model && lhs.uvTransform == rhs.uvTransform && lhs.tint == rhs.tint;
        }
};
