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
        config.graphics.backend = GraphicsBackend::Vulkan;
        config.graphics.headless = true;
        config.graphics.enableValidationLayers = false;
        config.graphics.renderWidth = 64;
        config.graphics.renderHeight = 64;
        config.graphics.maxFrameRate = 0;
        config.simulationTickRate = 60;
        config.debugOverlayEnabledByDefault = false;

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
