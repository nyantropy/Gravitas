#pragma once

#include <vulkan/vulkan.h>
#include <cstdio>
#include <unordered_map>
#include <memory>
#include <stdexcept>

#include "vcsheet.h"
#include "dssheet.h"
#include "BufferUtil.hpp"
#include "CameraBufferResource.h"
#include "CameraUBO.h"
#include "GraphicsConstants.h"
#include "Types.h"

// Manages camera (view) UBOs exclusively.
//
// One render view = one CameraBufferResource containing:
//   - framesInFlight UBOs, persistently mapped
//   - framesInFlight descriptor sets (set 0, camera UBO layout)
//
// The manager is intentionally unaware of per-object data; that lives in
// ObjectSSBOManager.  A scene with multiple cameras creates multiple views
// and binds the appropriate one during recording.
class CameraBufferManager
{
    public:
        CameraBufferManager() = default;

        ~CameraBufferManager()
        {
            VkDevice device = vcsheet::getDevice();
            for (auto& [id, res] : idToView)
            {
                for (size_t i = 0; i < res->uniformBuffers.size(); ++i)
                {
                    if (res->uniformBuffers[i] != VK_NULL_HANDLE)
                        vkDestroyBuffer(device, res->uniformBuffers[i], nullptr);
                    if (res->uniformBuffersMemory[i] != VK_NULL_HANDLE)
                        vkFreeMemory(device, res->uniformBuffersMemory[i], nullptr);
                }
            }
            idToView.clear();
        }

        // Allocates UBOs and a descriptor set for a new render view.
        // Returns a stable view_id_type handle; 0 is reserved as invalid.
        view_id_type createView()
        {
            auto res = std::make_unique<CameraBufferResource>();

            BufferUtil::createUniformBuffers(
                vcsheet::getDevice(),
                vcsheet::getPhysicalDevice(),
                res->uniformBuffers,
                res->uniformBuffersMemory,
                res->uniformBuffersMapped,
                GraphicsConstants::MAX_FRAMES_IN_FLIGHT,
                sizeof(CameraUBO)
            );

            res->descriptorSets = dssheet::getManager()
                .allocateForUniformBuffer(res->uniformBuffers, sizeof(CameraUBO));

            view_id_type id = nextID++;
            allocatedViews += 1;
            allocatedDescriptorSets += res->descriptorSets.size();
            idToView[id] = std::move(res);
            std::printf("[camera] allocate view=%u liveViews=%zu allocatedViews=%zu allocatedSets=%zu freedSets=%zu\n",
                        static_cast<unsigned>(id),
                        idToView.size(),
                        allocatedViews,
                        allocatedDescriptorSets,
                        freedDescriptorSets);
            return id;
        }

        // Frees the GPU resources for the given view.
        void destroyView(view_id_type id)
        {
            auto it = idToView.find(id);
            if (it == idToView.end())
                return;

            VkDevice device = vcsheet::getDevice();
            auto& res = it->second;

            // Return descriptor sets to the UBO pool before freeing the buffers.
            // The pool is created with FREE_DESCRIPTOR_SET_BIT specifically for this.
            if (!res->descriptorSets.empty())
            {
                dssheet::getManager().freeUniformBufferSets(res->descriptorSets);
                freedDescriptorSets += res->descriptorSets.size();
            }

            for (size_t i = 0; i < res->uniformBuffers.size(); ++i)
            {
                if (res->uniformBuffers[i] != VK_NULL_HANDLE)
                    vkDestroyBuffer(device, res->uniformBuffers[i], nullptr);
                if (res->uniformBuffersMemory[i] != VK_NULL_HANDLE)
                    vkFreeMemory(device, res->uniformBuffersMemory[i], nullptr);
            }
            idToView.erase(it);
            destroyedViews += 1;
            std::printf("[camera] free view=%u liveViews=%zu destroyedViews=%zu allocatedSets=%zu freedSets=%zu\n",
                        static_cast<unsigned>(id),
                        idToView.size(),
                        destroyedViews,
                        allocatedDescriptorSets,
                        freedDescriptorSets);
        }

        // Returns the GPU resource for the given view, or nullptr if invalid.
        CameraBufferResource* getView(view_id_type id)
        {
            auto it = idToView.find(id);
            return (it != idToView.end()) ? it->second.get() : nullptr;
        }

        // Writes view/proj matrices to every UBO slot for this render view.
        void uploadView(view_id_type id, const glm::mat4& view, const glm::mat4& proj)
        {
            CameraBufferResource* res = getView(id);
            if (!res) return;

            CameraUBO ubo;
            ubo.view = view;
            ubo.proj = proj;

            for (void* mapped : res->uniformBuffersMapped)
                memcpy(mapped, &ubo, sizeof(CameraUBO));
        }

    private:
        std::unordered_map<view_id_type, std::unique_ptr<CameraBufferResource>> idToView;
        view_id_type nextID = 1; // 0 = invalid
        size_t allocatedViews = 0;
        size_t destroyedViews = 0;
        size_t allocatedDescriptorSets = 0;
        size_t freedDescriptorSets = 0;
};
