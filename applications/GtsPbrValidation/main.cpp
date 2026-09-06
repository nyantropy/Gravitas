#include <cstdlib>
#include <memory>

#include "EngineConfig.h"
#include "GravitasEngine.hpp"
#include "PbrValidationScene.hpp"

int main()
{
    EngineConfig config;
    config.graphics.startup.backend = GraphicsBackend::Vulkan;
    config.graphics.settings.window.width = 1280;
    config.graphics.settings.window.height = 720;
    config.graphics.windowTitle = "Gravitas PBR Validation";
    config.graphics.settings.window.windowMode = WindowMode::Windowed;
    config.graphics.settings.presentation.mode = PresentModePreference::Fifo;

    GravitasEngine engine(config);
    engine.registerScene(
        "pbr_validation",
        []()
        {
            return std::make_unique<PbrValidationScene>();
        });
    engine.setActiveScene("pbr_validation");
    engine.start();

    return EXIT_SUCCESS;
}

