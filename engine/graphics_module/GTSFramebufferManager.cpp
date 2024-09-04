#include "GTSFramebufferManager.hpp"

GTSFramebufferManager::GTSFramebufferManager(VulkanLogicalDevice* vlogicaldevice, VulkanSwapChain* vswapchain, VulkanRenderer* vrenderer, VulkanRenderPass* vrenderpass)
{
    this->vlogicaldevice = vlogicaldevice;
    this->vswapchain = vswapchain;
    this->vrenderer = vrenderer;
    this->vrenderpass = vrenderpass;

    createFramebuffers();
}

GTSFramebufferManager::~GTSFramebufferManager()
{
    for (auto framebuffer : swapChainFramebuffers) 
    {
        vkDestroyFramebuffer(vlogicaldevice->getDevice(), framebuffer, nullptr);
    }
}

std::vector<VkFramebuffer>& GTSFramebufferManager::getFramebuffers()
{
    return swapChainFramebuffers;
}

void GTSFramebufferManager::createFramebuffers() 
{
    swapChainFramebuffers.resize(vswapchain->getSwapChainImageViews().size());

    for (size_t i = 0; i < vswapchain->getSwapChainImageViews().size(); i++) 
    {
        std::array<VkImageView, 2> attachments = 
        {
            vswapchain->getSwapChainImageViews()[i],
            vrenderer->getDepthImageView()
        };

        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = vrenderpass->getRenderPass();
        framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        framebufferInfo.pAttachments = attachments.data();
        framebufferInfo.width = vswapchain->getSwapChainExtent().width;
        framebufferInfo.height = vswapchain->getSwapChainExtent().height;
        framebufferInfo.layers = 1;

        if (vkCreateFramebuffer(vlogicaldevice->getDevice(), &framebufferInfo, nullptr, &swapChainFramebuffers[i]) != VK_SUCCESS) 
        {
            throw std::runtime_error("failed to create framebuffer!");
        }
    }
}