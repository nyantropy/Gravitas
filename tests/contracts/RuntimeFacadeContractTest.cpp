#include "GravitasEngine.hpp"
#include "rendering/GraphicsBackendInstaller.h"

#include <stdexcept>
#include <type_traits>

static_assert(std::is_constructible_v<GravitasEngine, const EngineConfig&>);

int main()
{
    const EngineConfig config;
    if (config.graphics.startup.backend != GraphicsBackend::Vulkan)
        throw std::runtime_error("Complete runtime must default to Vulkan");

    gts::rendering::GraphicsBackendRegistry first;
    gts::rendering::installDefaultGraphicsBackends(first);
    const auto* provider = first.find(GraphicsBackend::Vulkan);
    if (provider == nullptr)
        throw std::runtime_error("Runtime did not install its Vulkan provider");

    gts::rendering::GraphicsBackendRegistry second;
    gts::rendering::installDefaultGraphicsBackends(second);
    gts::rendering::installDefaultGraphicsBackends(first);
    if (second.find(GraphicsBackend::Vulkan) != provider || first.find(GraphicsBackend::Vulkan) != provider)
        throw std::runtime_error("Default installation changed provider lifetime or identity");
}
