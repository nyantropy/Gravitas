#include <iostream>
#include <filesystem>
#include <vector>
#include <iostream>

#include "GravitasEngine.hpp"
#include "EngineConfig.h"
#include "TetrisScene.hpp"

int main()
{
    EngineConfig config;
    config.graphics.backend                     = GraphicsBackend::Vulkan;
    config.graphics.window.width                = 1920;
    config.graphics.window.height               = 1080;
    config.graphics.window.title                = "GravitasTetris";
    config.graphics.window.borderlessFullscreen = true;
    config.graphics.window.vsync                = true;
    config.graphics.window.monitorIndex         = 0;

    GravitasEngine engine(config);
    engine.registerScene("tetris", std::make_unique<TetrisScene>());
    engine.setActiveScene("tetris");
    engine.start();

    return EXIT_SUCCESS;
}
