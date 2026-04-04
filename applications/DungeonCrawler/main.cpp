#include <iostream>
#include <filesystem>
#include <vector>

#include "GravitasEngine.hpp"
#include "EngineConfig.h"
#include "BattleScene.h"
#include "DungeonFloorScene.h"
#include "DungeonTestScene.h"
#include "StairTestScene.h"

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

    engine.registerScene("stair_test", std::make_unique<StairTestScene>());
    engine.registerScene("dungeon",    std::make_unique<DungeonFloorScene>());
    engine.registerScene("dungeon_test", std::make_unique<DungeonTestScene>());
    engine.registerScene("battle", std::make_unique<BattleScene>());
    engine.setActiveScene("dungeon_test");
    engine.start();

    return EXIT_SUCCESS;
}
