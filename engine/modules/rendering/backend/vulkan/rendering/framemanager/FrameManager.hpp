#pragma once

#include "FrameResources.h"
#include "vcsheet.h"
#include "GraphicsConstants.h"

#include <vulkan/vulkan.h>
#include <vector>
#include <stdexcept>

class FrameManager 
{
    public:
        explicit FrameManager(size_t frameOutputImageCount, bool allocateRenderFinishedSemaphores = true);
        ~FrameManager();

        FrameResources& getFrame(size_t frameIndex) { return frames[frameIndex]; }
        size_t getFrameCount() const { return frames.size(); }
        VkSemaphore getRenderFinishedSemaphore(size_t imageIndex) { return renderFinishedSemaphores[imageIndex]; }
        VkFence getImageFence(size_t imageIndex) { return imagesInFlight[imageIndex]; }
        size_t getImagesInFlightCount() { return imagesInFlight.size(); }
        void setImageFence(size_t imageIndex, VkFence fence) 
        {
            if (imageIndex >= imagesInFlight.size())
            {
                return;
            }
            
            imagesInFlight[imageIndex] = fence;
        }


    private:
        void createFrameResources();
        VkSemaphore createSemaphore();
        VkFence createFence();
        VkCommandBuffer allocateCommandBuffer();
        size_t frameOutputImageCount = 0;
        bool allocateRenderFinishedSemaphores = true;

        std::vector<FrameResources> frames;                // based per frame
        std::vector<VkSemaphore> renderFinishedSemaphores; // based on frame output image
        std::vector<VkFence> imagesInFlight;               // based on frame output image
};
