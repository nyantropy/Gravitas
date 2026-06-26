#pragma once

#include "GraphicsBackendRegistry.h"

namespace gts::rendering
{
    class VulkanGraphicsBackendProvider : public IGraphicsBackendProvider
    {
    public:
        GraphicsBackend backend() const override;
        std::unique_ptr<IGtsGraphicsModule> create(const GraphicsConfig& config) const override;
    };
}
