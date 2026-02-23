#pragma once
#include <string>

#include "IResourceProvider.hpp"
#include "MeshManager.hpp"
#include "TextureManager.hpp"
#include "UniformBufferManager.hpp"
#include "ObjectSSBOManager.hpp"
#include "Types.h"

class RenderResourceManager : public IResourceProvider
{
    private:
        std::unique_ptr<DescriptorSetManager>  descriptorSetManager;
        std::unique_ptr<MeshManager>           meshManager;
        std::unique_ptr<UniformBufferManager>  uniformBufferManager;
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
            uniformBufferManager = std::make_unique<UniformBufferManager>();
            textureManager       = std::make_unique<TextureManager>();

            // ObjectSSBOManager allocates SSBO buffers and their descriptor sets;
            // must come after dssheet is set.
            objectSSBOManager    = std::make_unique<ObjectSSBOManager>();
        }

        ~RenderResourceManager()
        {
            // Destroy in reverse-construction order; descriptorSetManager last
            // so the pool stays alive while other managers free their sets.
            if (objectSSBOManager)    objectSSBOManager.reset();
            if (textureManager)       textureManager.reset();
            if (uniformBufferManager) uniformBufferManager.reset();
            if (meshManager)          meshManager.reset();
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

        // --- Texture ---
        texture_id_type requestTexture(const std::string& path) override
        {
            return textureManager->loadTexture(path);
        }

        TextureResource* getTexture(texture_id_type id)
        {
            return textureManager->getTexture(id);
        }

        // --- Camera UBO (still per-camera, per-frame-in-flight) ---
        uniform_id_type requestUniformBuffer(size_t size) override
        {
            return uniformBufferManager->createUniformBuffer(static_cast<VkDeviceSize>(size));
        }

        UniformBufferResource* getUniformBuffer(uniform_id_type id)
        {
            return uniformBufferManager->getUniformBuffer(id);
        }

        // --- Object SSBO slot management ---
        uint32_t requestObjectSlot() override
        {
            return objectSSBOManager->requestSlot();
        }

        void releaseObjectSlot(uint32_t slot) override
        {
            objectSSBOManager->releaseSlot(slot);
        }

        void writeObjectSlot(uint32_t frameIndex, uint32_t slot, const ObjectUBO& data)
        {
            objectSSBOManager->writeSlot(frameIndex, slot, data);
        }

        VkDescriptorSet getObjectSSBODescriptorSet(uint32_t frameIndex)
        {
            return objectSSBOManager->getDescriptorSet(frameIndex);
        }
};
