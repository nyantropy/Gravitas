#include "VulkanGraphicsBackendProvider.h"
#include "VulkanBackendInstaller.h"

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

    void installVulkanGraphicsBackend(GraphicsBackendRegistry& registry)
    {
        // static lifetime keeps the registrys borrowed provider pointer valid
        static const VulkanGraphicsBackendProvider provider;
        registry.registerProvider(provider);
    }
}
