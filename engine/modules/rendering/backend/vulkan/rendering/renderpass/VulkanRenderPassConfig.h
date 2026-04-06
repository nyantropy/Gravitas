#pragma once

#include <vulkan/vulkan.h>

struct VulkanRenderPassConfig
{
    VkFormat colorFormat = VK_FORMAT_UNDEFINED;
    VkFormat depthFormat = VK_FORMAT_UNDEFINED;

    // ── color attachment behaviour ────────────────────────────────────────
    // Defaults reproduce the main render pass (clear on load, start from
    // undefined layout, end in present-ready layout).
    VkAttachmentLoadOp colorLoadOp       = VK_ATTACHMENT_LOAD_OP_CLEAR;
    VkImageLayout      colorInitialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    VkImageLayout      colorFinalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    // ── depth attachment ──────────────────────────────────────────────────
    // Set to false for passes that have no depth attachment (e.g. UI pass).
    bool hasDepthAttachment = true;
};
