#pragma once
#include <string>

#include "DescriptorSetManager.hpp"
#include "IResourceProvider.hpp"
#include "MeshManager.hpp"
#include "TextureManager.hpp"
#include "FontManager.hpp"
#include "CameraBufferManager.hpp"
#include "ObjectSSBOManager.hpp"
#include "Types.h"
#include "VulkanBackendContext.h"

class RenderResourceManager : public IResourceProvider
{
    private:
        std::unique_ptr<DescriptorSetManager>     descriptorSetManager;
        std::unique_ptr<MeshManager>              meshManager;
        std::unique_ptr<CameraBufferManager>      cameraBufferManager;
        std::unique_ptr<TextureManager>           textureManager;
        std::unique_ptr<FontManager>              fontManager;
        std::unique_ptr<ObjectSSBOManager>        objectSSBOManager;

    public:
        explicit RenderResourceManager(VulkanBackendContext& backendContext)
        {
            descriptorSetManager =
                std::make_unique<DescriptorSetManager>(backendContext, GraphicsConstants::MAX_FRAMES_IN_FLIGHT);

            meshManager          = std::make_unique<MeshManager>(backendContext);
            cameraBufferManager  = std::make_unique<CameraBufferManager>(backendContext, *descriptorSetManager);
            textureManager       = std::make_unique<TextureManager>(backendContext, *descriptorSetManager);
            fontManager          = std::make_unique<FontManager>(textureManager.get());

            objectSSBOManager    = std::make_unique<ObjectSSBOManager>(backendContext, *descriptorSetManager);
        }

        ~RenderResourceManager()
        {
            // Destroy in reverse-construction order; descriptorSetManager last
            // so the pool stays alive while other managers free their sets.
            if (objectSSBOManager)   objectSSBOManager.reset();
            if (fontManager)         fontManager.reset();
            if (textureManager)      textureManager.reset();
            if (cameraBufferManager) cameraBufferManager.reset();
            if (meshManager)         meshManager.reset();
            if (descriptorSetManager) descriptorSetManager.reset();
        }

        DescriptorSetManager& getDescriptorSetManager()
        {
            return *descriptorSetManager;
        }

        // --- Mesh ---
        mesh_id_type requestMesh(const std::string& path) override
        {
            return meshManager->loadMesh(path);
        }

        mesh_id_type getSharedQuadMesh(float w, float h) override
        {
            return meshManager->getOrCreateQuadMesh(w, h);
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

        void releaseProceduralMesh(mesh_id_type id) override
        {
            meshManager->destroyProceduralMesh(id);
        }

        // --- Texture ---
        texture_id_type requestTexture(const std::string& path) override
        {
            return textureManager->loadTexture(path);
        }

        texture_id_type requestClampedTexture(const std::string& path) override
        {
            return textureManager->loadTexture(path, false, true);
        }

        texture_id_type requestPixelTexture(const std::string& path) override
        {
            return textureManager->loadTexture(path, true);
        }

        TextureDimensions getTextureDimensions(texture_id_type id) const override
        {
            if (!textureManager)
                return {};

            return {
                textureManager->getTextureWidth(id),
                textureManager->getTextureHeight(id)
            };
        }

        TextureResource* getTexture(texture_id_type id)
        {
            return textureManager->getTexture(id);
        }

        // --- Font ---
        font_id_type requestFont(const std::string& path) override
        {
            return fontManager->loadFont(path);
        }

        const BitmapFont* getFont(font_id_type id) const override
        {
            return fontManager->getFont(id);
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
        // Called by CameraBindingSystem.
        void uploadCameraView(view_id_type id, const glm::mat4& view, const glm::mat4& proj) override
        {
            cameraBufferManager->uploadView(id, view, proj);
        }

        void uploadCameraViewFrame(uint32_t frameIndex,
                                   view_id_type id,
                                   const glm::mat4& view,
                                   const glm::mat4& proj)
        {
            cameraBufferManager->uploadViewFrame(frameIndex, id, view, proj);
        }

        // Backend-only: returns raw GPU resource for descriptor-set binding.
        // Called only from ForwardRenderer (backend), which holds RenderResourceManager*.
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

        bool writeObjectSlot(uint32_t frameIndex, ssbo_id_type slot, const ObjectUBO& data)
        {
            return objectSSBOManager->writeSlot(frameIndex, slot, data);
        }

        uint32_t writeObjectSlotAllFrames(ssbo_id_type slot, const ObjectUBO& data)
        {
            return objectSSBOManager->writeSlotAllFrames(slot, data);
        }

        void flushObjectSSBO(uint32_t frameIndex)
        {
            objectSSBOManager->flushFrame(frameIndex);
        }

        void flushAllObjectSSBO()
        {
            objectSSBOManager->flushAllFrames();
        }

        VkDescriptorSet getObjectSSBODescriptorSet(uint32_t frameIndex)
        {
            return objectSSBOManager->getDescriptorSet(frameIndex);
        }
};
