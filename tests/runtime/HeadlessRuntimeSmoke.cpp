#include <cstdlib>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

#include "EngineConfig.h"
#include "GravitasEngine.hpp"

class HeadlessRuntimeSmokeScene : public GtsScene
{
public:
    void onLoad(EcsControllerContext&, const GtsSceneTransitionData*) override
    {
    }

    void onUpdateSimulation(const EcsSimulationContext&) override
    {
    }

    void onUpdateControllers(const EcsControllerContext& ctx) override
    {
        if (ctx.engineCommands != nullptr)
            ctx.engineCommands->requestQuit();
    }
};

int main()
{
    try
    {
        EngineConfig config;
        config.graphics.startup.backend = GraphicsBackend::Vulkan;
        config.graphics.startup.headless = true;
        config.graphics.startup.enableValidationLayers = false;
        config.graphics.settings.rendering.resolution.mode = RenderResolutionMode::Fixed;
        config.graphics.settings.rendering.resolution.fixedExtent.width = 64;
        config.graphics.settings.rendering.resolution.fixedExtent.height = 64;
        config.graphics.settings.framePacing.maxFrameRate = 0;
        config.simulation.tickRate = 60;
        config.tools.debugOverlayEnabledByDefault = false;

        GravitasEngine engine(config);
        engine.registerScene("headless_smoke",
                             []()
                             {
                                 return std::make_unique<HeadlessRuntimeSmokeScene>();
                             });
        engine.setActiveScene("headless_smoke");
        engine.start();
    }
    catch (const std::runtime_error& error)
    {
        const std::string message = error.what();
        if (message.find("failed to find GPUs with Vulkan support") != std::string::npos)
        {
            std::cout << "Skipping headless runtime smoke: no Vulkan-capable GPU available." << std::endl;
            return 77;
        }

        throw;
    }

    return EXIT_SUCCESS;
}
