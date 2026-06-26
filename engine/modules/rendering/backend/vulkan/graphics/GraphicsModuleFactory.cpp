#include "GraphicsModuleFactory.h"

#include "VulkanGraphics.hpp"

namespace gts::rendering
{
    std::unique_ptr<IGtsGraphicsModule> createGraphicsModule(const GraphicsConfig& config)
    {
        switch (config.backend)
        {
            case GraphicsBackend::Vulkan:
                return std::make_unique<VulkanGraphics>(config);
        }

        return nullptr;
    }
}
