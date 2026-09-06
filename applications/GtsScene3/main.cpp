#include <memory>

#include "GravitasEngine.hpp"
#include "EngineConfig.h"
#include "GtsScene3.hpp"

int main()
{
    EngineConfig config;
    config.graphics.startup.backend                = GraphicsBackend::Vulkan;
    config.graphics.settings.window.width           = 1600;
    config.graphics.settings.window.height          = 900;
    config.graphics.windowTitle           = "GtsScene3 - Stress Test";
    config.graphics.settings.window.windowMode      = WindowMode::Windowed;
    config.graphics.settings.presentation.mode = PresentModePreference::Immediate;
    config.graphics.startup.enableValidationLayers = false;
    config.graphics.settings.rendering.frustumCullingEnabled           = true;

    GravitasEngine engine(config);
    engine.registerScene("stress",
                         []()
                         {
                             return std::make_unique<GtsScene3>();
                         });
    engine.setActiveScene("stress");
    engine.start();

    return EXIT_SUCCESS;
}
