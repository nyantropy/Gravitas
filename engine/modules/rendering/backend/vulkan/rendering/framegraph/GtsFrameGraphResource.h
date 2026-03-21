#pragma once

#include <vulkan/vulkan.h>
#include <string>

// Identifies the category of a frame graph resource.
enum class GtsResourceType
{
    Imported,   // External persistent resource (swapchain image, depth attachment).
                // Owned outside the graph; the graph only tracks layout.
    Transient   // Temporary render target created and destroyed by the graph.
                // Lives only for the duration of one compiled frame graph.
};

// Describes how a stage accesses a resource.
struct GtsResourceAccess
{
    VkPipelineStageFlags stageMask;
    VkAccessFlags        accessMask;
    VkImageLayout        layout;
};

// A frame graph resource handle.
// Stages reference resources by handle returned from importResource /
// createTransientResource.  The graph owns layout tracking.
struct GtsFrameGraphResource
{
    std::string        name;
    GtsResourceType    type;
    VkFormat           format;
    VkExtent2D         extent;
    VkImageUsageFlags  usage;
    VkImageAspectFlags aspectMask;

    // Current tracked layout — updated by the graph as stages execute.
    VkImageLayout currentLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    // Vulkan handles — set for imported resources; allocated by graph for transient.
    VkImage        image     = VK_NULL_HANDLE;
    VkImageView    imageView = VK_NULL_HANDLE;
    VkDeviceMemory memory    = VK_NULL_HANDLE;  // only for transient resources
};

// Index into GtsFrameGraph's resource list.
using GtsResourceHandle = uint32_t;
static constexpr GtsResourceHandle GTS_INVALID_RESOURCE = UINT32_MAX;
