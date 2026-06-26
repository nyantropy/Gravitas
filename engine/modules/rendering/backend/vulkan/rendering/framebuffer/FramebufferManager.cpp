#include "FramebufferManager.hpp"

#include <algorithm>

// short summary, because i will forget in due time:
// we need to combine the frame output image view (which is the image we will present or capture)
// with the attachment image view (right now, thats a depth image by implementation)
// Why do we need to do that though?
// - Because a framebuffer MUST include ALL attachments used by the render pass
// that means, since we have 2 attachments right now (color and depth)
// the framebuffer needs to provide (surprise surprise) 2 imageviews
FramebufferManager::FramebufferManager(VulkanBackendContext& backendContext, const FramebufferManagerConfig& config)
    : backendContext(backendContext)
{
    this->config = config;

    if (backendContext.frameOutputImageViews().empty())
        throw std::runtime_error("VulkanFramebufferManager: no frame output image views available.");

    const auto& defaultColorViews = backendContext.frameOutputImageViews();
    const std::vector<VkImageView>& colorViews = config.colorImageViews.empty()
        ? defaultColorViews
        : config.colorImageViews;
    if (colorViews.empty())
        throw std::runtime_error("VulkanFramebufferManager: no color image views available.");

    const uint32_t framebufferCount = config.framebufferCount > 0
        ? config.framebufferCount
        : static_cast<uint32_t>(std::max(defaultColorViews.size(), colorViews.size()));

    VulkanFramebufferSetConfig fbConfig{};
    fbConfig.renderPass = config.vkRenderpass;
    fbConfig.width = config.width > 0 ? config.width : backendContext.frameOutputExtent().width;
    fbConfig.height = config.height > 0 ? config.height : backendContext.frameOutputExtent().height;
    fbConfig.layers = 1;

    fbConfig.attachmentsPerFramebuffer.resize(framebufferCount);
    for (uint32_t i = 0; i < framebufferCount; ++i)
    {
        const VkImageView colorView = colorViews.size() == 1
            ? colorViews[0]
            : colorViews[i % colorViews.size()];

        if (config.hasDepthAttachment)
        {
            // color views are unique per framebuffer
            // attachment views (depth) remains the same for all frames in flight
            fbConfig.attachmentsPerFramebuffer[i] =
            {
                colorView,
                config.attachmentImageView
            };
        }
        else
        {
            fbConfig.attachmentsPerFramebuffer[i] =
            {
                colorView
            };
        }
    }

    framebuffers = std::make_unique<VulkanFramebufferSet>(backendContext, fbConfig);
}
