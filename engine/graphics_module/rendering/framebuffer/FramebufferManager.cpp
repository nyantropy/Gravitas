#include "FramebufferManager.hpp"

// short summary, because i will forget in due time:
// we need to combine the swapchain image view (which is the image we will present to the screen)
// with the attachment image view (right now, thats a depth image by implementation)
// Why do we need to do that though?
// - Because a framebuffer MUST include ALL attachments used by the render pass
// that means, since we have 2 attachments right now (color and depth)
// the framebuffer needs to provide (surprise surprise) 2 imageviews
FramebufferManager::FramebufferManager(const FramebufferManagerConfig& config)
{
    this->config = config;

    if (vcsheet::getSwapChainImageViews().empty())
        throw std::runtime_error("VulkanFramebufferManager: no swapchain image views available.");

    VulkanFramebufferSetConfig fbConfig{};
    fbConfig.renderPass = config.vkRenderpass;
    fbConfig.width = vcsheet::getSwapChainExtent().width;
    fbConfig.height = vcsheet::getSwapChainExtent().height;
    fbConfig.layers = 1;

    fbConfig.attachmentsPerFramebuffer.resize(vcsheet::getSwapChainImageViews().size());
    for (size_t i = 0; i < vcsheet::getSwapChainImageViews().size(); ++i)
    {
        // color views are unique per framebuffer
        // attachment views (depth) remains the same for all frames in flight
        fbConfig.attachmentsPerFramebuffer[i] = 
        {
            vcsheet::getSwapChainImageViews()[i],
            config.attachmentImageView
        };
    }

    framebuffers = std::make_unique<VulkanFramebufferSet>(fbConfig);
}
