#pragma once
#include <string>

#include "IResourceProvider.hpp"
#include "MeshManager.hpp"
#include "TextureManager.hpp"
#include "CameraBufferManager.hpp"
#include "ObjectSSBOManager.hpp"
#include "Types.h"

class RenderResourceManager : public IResourceProvider
{
    private:
        std::unique_ptr<DescriptorSetManager>  descriptorSetManager;
        std::unique_ptr<MeshManager>           meshManager;
        std::unique_ptr<CameraBufferManager>   cameraBufferManager;
        std::unique_ptr<TextureManager>        textureManager;
        std::unique_ptr<ObjectSSBOManager>     objectSSBOManager;

    public:
        RenderResourceManager()
        {
            // DescriptorSetManager must be created and registered first so that
            // subsequent managers can allocate descriptor sets via dssheet.
            descriptorSetManager = std::make_unique<DescriptorSetManager>(GraphicsConstants::MAX_FRAMES_IN_FLIGHT);
            dssheet::SetManager(descriptorSetManager.get());

            meshManager          = std::make_unique<MeshManager>();
            cameraBufferManager  = std::make_unique<CameraBufferManager>();
            textureManager       = std::make_unique<TextureManager>();

            // ObjectSSBOManager allocates SSBO buffers and their descriptor sets;
            // must come after dssheet is set.
            objectSSBOManager    = std::make_unique<ObjectSSBOManager>();
        }

        ~RenderResourceManager()
        {
            // Destroy in reverse-construction order; descriptorSetManager last
            // so the pool stays alive while other managers free their sets.
            if (objectSSBOManager)   objectSSBOManager.reset();
            if (textureManager)      textureManager.reset();
            if (cameraBufferManager) cameraBufferManager.reset();
            if (meshManager)         meshManager.reset();
            if (descriptorSetManager) descriptorSetManager.reset();
        }

        // --- Mesh ---
        mesh_id_type requestMesh(const std::string& path) override
        {
            return meshManager->loadMesh(path);
        }

        MeshResource* getMesh(mesh_id_type id)
        {
            return meshManager->getMesh(id);
        }

        mesh_id_type uploadProceduralMesh(mesh_id_type                 existingId,
                                          const std::vector<Vertex>&   vertices,
                                          const std::vector<uint32_t>& indices) override
        {
            return meshManager->uploadProceduralMesh(existingId, vertices, indices);
        }

        // --- Texture ---
        texture_id_type requestTexture(const std::string& path) override
        {
            return textureManager->loadTexture(path);
        }

        TextureResource* getTexture(texture_id_type id)
        {
            return textureManager->getTexture(id);
        }

        // --- Camera / render-view buffer management ---
        view_id_type requestCameraBuffer() override
        {
            return cameraBufferManager->createView();
        }

        void releaseCameraBuffer(view_id_type id) override
        {
            cameraBufferManager->destroyView(id);
        }

        // Writes view/proj to every frames-in-flight UBO slot for this view.
        // Called by CameraBindingSystem; the renderer only needs to bind the
        // descriptor set — it no longer owns the camera UBO upload.
        void uploadCameraView(view_id_type id, const glm::mat4& view, const glm::mat4& proj) override
        {
            CameraBufferResource* res = cameraBufferManager->getView(id);
            if (!res) return;

            CameraUBO ubo;
            ubo.view = view;
            ubo.proj = proj;

            for (void* mapped : res->uniformBuffersMapped)
                memcpy(mapped, &ubo, sizeof(CameraUBO));
        }

        CameraBufferResource* getCameraView(view_id_type id)
        {
            return cameraBufferManager->getView(id);
        }

        // --- Object SSBO slot management ---
        ssbo_id_type requestObjectSlot() override
        {
            return objectSSBOManager->requestSlot();
        }

        void releaseObjectSlot(ssbo_id_type slot) override
        {
            objectSSBOManager->releaseSlot(slot);
        }

        void writeObjectSlot(uint32_t frameIndex, ssbo_id_type slot, const ObjectUBO& data)
        {
            objectSSBOManager->writeSlot(frameIndex, slot, data);
        }

        VkDescriptorSet getObjectSSBODescriptorSet(uint32_t frameIndex)
        {
            return objectSSBOManager->getDescriptorSet(frameIndex);
        }
};
