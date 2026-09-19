#pragma once

namespace gts::rendering
{
    class GraphicsBackendRegistry;

    // Supplied by gravitas_vulkan_backend; provider lifetime outlives the registry.
    void installVulkanGraphicsBackend(GraphicsBackendRegistry& registry);
}
