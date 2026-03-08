#include <iostream>
#include <filesystem>
#include <vector>

#include "GravitasEngine.hpp"
#include "EngineConfig.h"
#include "DefaultScene.hpp"

int main()
{
    EngineConfig config;
    config.graphics.backend                     = GraphicsBackend::Vulkan;
    config.graphics.window.width                = 800;
    config.graphics.window.height               = 800;
    config.graphics.window.title                = "GtsScene1";
    config.graphics.window.borderlessFullscreen = false;
    config.graphics.window.vsync                = true;

    GravitasEngine engine(config);
    engine.registerScene("default", std::make_unique<DefaultScene>());
    engine.setActiveScene("default");
    engine.start();

    return EXIT_SUCCESS;
}
