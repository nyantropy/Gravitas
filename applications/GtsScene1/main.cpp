#include <iostream>
#include <filesystem>
#include <memory>
#include <vector>

#include "GravitasEngine.hpp"
#include "EngineConfig.h"
#include "DefaultScene.hpp"

int main()
{
    EngineConfig config;
    config.graphics.startup.backend           = GraphicsBackend::Vulkan;
    config.graphics.settings.window.width      = 800;
    config.graphics.settings.window.height     = 800;
    config.graphics.windowTitle      = "GtsScene1";
    config.graphics.settings.window.windowMode = WindowMode::Windowed;
    config.graphics.settings.presentation.mode = PresentModePreference::Fifo;

    GravitasEngine engine(config);
    engine.registerScene("default",
                         []()
                         {
                             return std::make_unique<DefaultScene>();
                         });
    engine.setActiveScene("default");
    engine.start();

    return EXIT_SUCCESS;
}
