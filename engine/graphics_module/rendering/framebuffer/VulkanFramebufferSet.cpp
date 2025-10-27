#include "VulkanFramebufferSet.hpp"

VulkanFramebufferSet::VulkanFramebufferSet(const VulkanFramebufferSetConfig& config)
{
    this->config = config;

    if (vcsheet::getDevice() == VK_NULL_HANDLE)
        throw std::runtime_error("VulkanFramebufferSet: invalid VkDevice in config.");
    if (config.renderPass == VK_NULL_HANDLE)
        throw std::runtime_error("VulkanFramebufferSet: invalid VkRenderPass in config.");
    if (config.attachmentsPerFramebuffer.empty())
        throw std::runtime_error("VulkanFramebufferSet: no attachments provided.");

    createFramebuffers();
}

VulkanFramebufferSet::~VulkanFramebufferSet()
{
    if (vcsheet::getDevice() == VK_NULL_HANDLE)
        return;

    for (auto fb : framebuffers)
    {
        if (fb != VK_NULL_HANDLE)
            vkDestroyFramebuffer(vcsheet::getDevice(), fb, nullptr);
    }

    framebuffers.clear();
}

void VulkanFramebufferSet::createFramebuffers()
{
    framebuffers.resize(config.attachmentsPerFramebuffer.size());

    for (size_t i = 0; i < config.attachmentsPerFramebuffer.size(); ++i)
    {
        const auto& attachments = config.attachmentsPerFramebuffer[i];

        if (attachments.empty())
        {
            throw std::runtime_error("VulkanFramebufferSet: framebuffer has no attachments.");
        }

        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = config.renderPass;
        framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        framebufferInfo.pAttachments = attachments.data();
        framebufferInfo.width = config.width;
        framebufferInfo.height = config.height;
        framebufferInfo.layers = config.layers;

        if (vkCreateFramebuffer(vcsheet::getDevice(), &framebufferInfo, nullptr, &framebuffers[i]) != VK_SUCCESS)
        {
            throw std::runtime_error("VulkanFramebufferSet: failed to create framebuffer.");
        }
    }
}