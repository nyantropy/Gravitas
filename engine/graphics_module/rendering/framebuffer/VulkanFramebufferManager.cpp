#include "VulkanFramebufferManager.hpp"

// short summary, because i will forget in due time:
// we need to combine the swapchain image view (which is the image we will present to the screen)
// with the attachment image view (right now, thats a depth image by implementation)
// Why do we need to do that though?
// - Because a framebuffer MUST include ALL attachments used by the render pass
// that means, since we have 2 attachments right now (color and depth)
// the framebuffer needs to provide (surprise surprise) 2 imageviews
VulkanFramebufferManager::VulkanFramebufferManager(const VulkanFramebufferManagerConfig& config)
{
    this->config = config;

    if (config.swapchainImageViews.empty())
        throw std::runtime_error("VulkanFramebufferManager: no swapchain image views available.");

    VulkanFramebufferConfig fbConfig{};
    fbConfig.device = config.vkDevice;
    fbConfig.renderPass = config.vkRenderpass;
    fbConfig.width = config.vkExtent.width;
    fbConfig.height = config.vkExtent.height;
    fbConfig.layers = 1;

    fbConfig.attachmentsPerFramebuffer.resize(config.swapchainImageViews.size());
    for (size_t i = 0; i < config.swapchainImageViews.size(); ++i)
    {
        // color views are unique per framebuffer
        // attachment views (depth) remains the same for all frames in flight
        fbConfig.attachmentsPerFramebuffer[i] = 
        {
            config.swapchainImageViews[i],
            config.attachmentImageView
        };
    }

    framebuffers = std::make_unique<VulkanFramebufferSet>(fbConfig);
}
