#include <iostream>
#include <filesystem>
#include <vector>

#include "GravitasEngine.hpp"
#include "EngineConfig.h"
#include "DungeonManager.h"

int main()
{
    EngineConfig config;
    config.graphics.backend              = GraphicsBackend::Vulkan;
    config.graphics.window.width         = 1920;
    config.graphics.window.height        = 1080;
    config.graphics.window.title         = "DungeonCrawler";
    config.graphics.window.windowMode    = WindowMode::BorderlessFullscreen;
    config.graphics.window.vsync         = true;

    GravitasEngine engine(config);

    DungeonManager dungeonManager;
    dungeonManager.setup(engine);

    engine.setActiveScene("floor_0");
    engine.start();

    return EXIT_SUCCESS;
}
