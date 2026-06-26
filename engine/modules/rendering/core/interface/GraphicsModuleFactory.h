#pragma once

#include <memory>

class IGtsGraphicsModule;
struct GraphicsConfig;

namespace gts::rendering
{
    std::unique_ptr<IGtsGraphicsModule> createGraphicsModule(const GraphicsConfig& config);
}
