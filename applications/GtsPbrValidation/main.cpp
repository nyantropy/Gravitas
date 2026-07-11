#include <cstdlib>
#include <memory>

#include "EngineConfig.h"
#include "GravitasEngine.hpp"
#include "PbrValidationScene.hpp"

int main()
{
    EngineConfig config;
    config.graphics.backend = GraphicsBackend::Vulkan;
    config.graphics.window.width = 1280;
    config.graphics.window.height = 720;
    config.graphics.window.title = "Gravitas PBR Validation";
    config.graphics.window.windowMode = WindowMode::Windowed;
    config.graphics.window.vsync = true;

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

