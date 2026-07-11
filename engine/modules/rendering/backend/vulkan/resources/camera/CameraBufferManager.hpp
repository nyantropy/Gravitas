#pragma once

#include <vulkan/vulkan.h>
#include <unordered_map>
#include <memory>
#include <stdexcept>

#include "DescriptorSetManager.hpp"
#include "VulkanBackendContext.h"
#include "BufferUtil.hpp"
#include "CameraBufferResource.h"
#include "CameraUBO.h"
#include "GraphicsConstants.h"
#include "LightingFrameData.h"
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
        CameraBufferManager(VulkanBackendContext& backendContext, DescriptorSetManager& descriptorSetManager)
            : backendContext(backendContext)
            , descriptorSetManager(descriptorSetManager)
        {
        }

        ~CameraBufferManager()
        {
            VkDevice device = backendContext.device();
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
                backendContext.device(),
                backendContext.physicalDevice(),
                res->uniformBuffers,
                res->uniformBuffersMemory,
                res->uniformBuffersMapped,
                GraphicsConstants::MAX_FRAMES_IN_FLIGHT,
                sizeof(CameraUBO)
            );

            res->descriptorSets = descriptorSetManager
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

            VkDevice device = backendContext.device();
            auto& res = it->second;

            // Return descriptor sets to the UBO pool before freeing the buffers.
            // The pool is created with FREE_DESCRIPTOR_SET_BIT specifically for this.
            if (!res->descriptorSets.empty())
                descriptorSetManager.freeUniformBufferSets(res->descriptorSets);

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

        // Writes view/proj matrices to every UBO slot for this render view.
        void uploadView(view_id_type id, const glm::mat4& view, const glm::mat4& proj)
        {
            CameraBufferResource* res = getView(id);
            if (!res) return;

            const CameraUBO ubo = makeCameraUbo(
                view,
                proj,
                {0.0f, 0.0f, 0.0f},
                gts::rendering::defaultLightingFrameData());

            for (void* mapped : res->uniformBuffersMapped)
                memcpy(mapped, &ubo, sizeof(CameraUBO));
        }

        // Writes view/proj matrices only to the frame slot whose fence has
        // already been waited by the renderer.
        void uploadViewFrame(uint32_t frameIndex,
                             view_id_type id,
                             const glm::mat4& view,
                             const glm::mat4& proj,
                             const glm::vec3& cameraWorldPosition,
                             const gts::rendering::LightingFrameData& lighting)
        {
            CameraBufferResource* res = getView(id);
            if (!res || frameIndex >= res->uniformBuffersMapped.size())
                return;

            const CameraUBO ubo = makeCameraUbo(view, proj, cameraWorldPosition, lighting);
            memcpy(res->uniformBuffersMapped[frameIndex], &ubo, sizeof(CameraUBO));
            res->environment = lighting.environment;
        }

    private:
        static CameraUBO makeCameraUbo(
            const glm::mat4& view,
            const glm::mat4& proj,
            const glm::vec3& cameraWorldPosition,
            const gts::rendering::LightingFrameData& lighting)
        {
            CameraUBO ubo{};
            ubo.view = view;
            ubo.proj = proj;
            ubo.cameraPosition = {cameraWorldPosition, 1.0f};
            ubo.lightingCountsAmbient = {
                static_cast<float>(lighting.directionalCount),
                static_cast<float>(lighting.pointCount),
                static_cast<float>(lighting.spotCount),
                lighting.ambientIntensity
            };
            ubo.environmentParameters = glm::vec4{
                gts::rendering::sanitizeEnvironmentIntensity(lighting.environment.intensity),
                gts::rendering::sanitizeEnvironmentRotation(lighting.environment.rotationRadians),
                lighting.environment.enabled ? 1.0f : 0.0f,
                0.0f
            };

            for (uint32_t i = 0; i < lighting.directionalCount && i < gts::rendering::MaxDirectionalLights; ++i)
            {
                const auto& light = lighting.directional[i];
                ubo.directional[i].directionIntensity = {light.directionToLight, light.intensity};
                ubo.directional[i].color = {light.color, 0.0f};
            }

            for (uint32_t i = 0; i < lighting.pointCount && i < gts::rendering::MaxPointLights; ++i)
            {
                const auto& light = lighting.point[i];
                ubo.point[i].positionRange = {light.position, light.range};
                ubo.point[i].colorIntensity = {light.color, light.intensity};
            }

            for (uint32_t i = 0; i < lighting.spotCount && i < gts::rendering::MaxSpotLights; ++i)
            {
                const auto& light = lighting.spot[i];
                ubo.spot[i].positionRange = {light.position, light.range};
                ubo.spot[i].directionIntensity = {light.directionFromLight, light.intensity};
                ubo.spot[i].colorInnerCone = {light.color, light.innerConeCos};
                ubo.spot[i].outerCone = {light.outerConeCos, 0.0f, 0.0f, 0.0f};
            }
            return ubo;
        }

        VulkanBackendContext& backendContext;
        DescriptorSetManager& descriptorSetManager;
        std::unordered_map<view_id_type, std::unique_ptr<CameraBufferResource>> idToView;
        view_id_type nextID = 1; // 0 = invalid
};
