#include "VulkanGraphicsBackendProvider.h"

#include "VulkanGraphics.hpp"

namespace gts::rendering
{
    GraphicsBackend VulkanGraphicsBackendProvider::backend() const
    {
        return GraphicsBackend::Vulkan;
    }

    std::unique_ptr<IGtsGraphicsModule> VulkanGraphicsBackendProvider::create(const GraphicsConfig& config) const
    {
        return std::make_unique<VulkanGraphics>(config);
    }

    void installDefaultGraphicsBackends(GraphicsBackendRegistry& registry)
    {
        static const VulkanGraphicsBackendProvider provider;
        registry.registerProvider(provider);
    }
}
