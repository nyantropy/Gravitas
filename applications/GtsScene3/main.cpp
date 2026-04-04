#include "GravitasEngine.hpp"
#include "EngineConfig.h"
#include "GtsScene3.hpp"

int main()
{
    EngineConfig config;
    config.graphics.backend                = GraphicsBackend::Vulkan;
    config.graphics.window.width           = 1600;
    config.graphics.window.height          = 900;
    config.graphics.window.title           = "GtsScene3 - Stress Test";
    config.graphics.window.windowMode      = WindowMode::Windowed;
    config.graphics.window.vsync           = false;
    config.graphics.enableValidationLayers = false;
    config.frustumCullingEnabled           = true;

    GravitasEngine engine(config);
    engine.registerScene("stress", std::make_unique<GtsScene3>());
    engine.setActiveScene("stress");
    engine.start();

    return EXIT_SUCCESS;
}
