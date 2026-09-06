#include <memory>

#include "GravitasEngine.hpp"
#include "EngineConfig.h"
#include "CullTestScene.hpp"

int main()
{
    EngineConfig config;
    config.graphics.startup.backend           = GraphicsBackend::Vulkan;
    config.graphics.settings.window.width      = 1280;
    config.graphics.settings.window.height     = 720;
    config.graphics.windowTitle      = "GtsScene2 — Frustum Culling Test";
    config.graphics.settings.window.windowMode = WindowMode::Windowed;
    config.graphics.settings.presentation.mode = PresentModePreference::Fifo;
    config.graphics.settings.rendering.frustumCullingEnabled      = true;

    GravitasEngine engine(config);
    engine.registerScene("culltest",
                         []()
                         {
                             return std::make_unique<CullTestScene>();
                         });
    engine.setActiveScene("culltest");
    engine.start();

    return EXIT_SUCCESS;
}
