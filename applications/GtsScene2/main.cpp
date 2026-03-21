#include "GravitasEngine.hpp"
#include "EngineConfig.h"
#include "CullTestScene.hpp"

int main()
{
    EngineConfig config;
    config.graphics.backend           = GraphicsBackend::Vulkan;
    config.graphics.window.width      = 1280;
    config.graphics.window.height     = 720;
    config.graphics.window.title      = "GtsScene2 — Frustum Culling Test";
    config.graphics.window.windowMode = WindowMode::Windowed;
    config.graphics.window.vsync      = true;
    config.frustumCullingEnabled      = true;

    GravitasEngine engine(config);
    engine.registerScene("culltest", std::make_unique<CullTestScene>());
    engine.setActiveScene("culltest");
    engine.start();

    return EXIT_SUCCESS;
}
