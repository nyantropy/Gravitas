#pragma once

#include <vulkan/vulkan.h>
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
        idToView[id] = std::move(res);
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
        for (size_t i = 0; i < res->uniformBuffers.size(); ++i)
        {
            if (res->uniformBuffers[i] != VK_NULL_HANDLE)
                vkDestroyBuffer(device, res->uniformBuffers[i], nullptr);
            if (res->uniformBuffersMemory[i] != VK_NULL_HANDLE)
                vkFreeMemory(device, res->uniformBuffersMemory[i], nullptr);
        }
        idToView.erase(it);
    }

    // Returns the GPU resource for the given view, or nullptr if invalid.
    CameraBufferResource* getView(view_id_type id)
    {
        auto it = idToView.find(id);
        return (it != idToView.end()) ? it->second.get() : nullptr;
    }

private:
    std::unordered_map<view_id_type, std::unique_ptr<CameraBufferResource>> idToView;
    view_id_type nextID = 1; // 0 = invalid
};
