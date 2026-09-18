#include <cstdlib>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

#include "EngineConfig.h"
#include "GravitasEngine.hpp"
#include "GraphicsConstants.h"
#include "BitmapFont.h"

class HeadlessRuntimeSmokeScene : public GtsScene
{
    bool toolsEnabled;
public:
    explicit HeadlessRuntimeSmokeScene(bool toolsEnabled) : toolsEnabled(toolsEnabled) {}

    void onLoad(EcsControllerContext& ctx, const GtsSceneTransitionData*) override
    {
        for (const char* action : {"engine.pause", "engine.ui_submit", "engine.zoom_in"})
        {
            if (ctx.input->getTriggersForAction(action).empty())
                throw std::runtime_error("Module input registration missing");
        }
        if (ctx.input->getTriggersForAction("engine.tools_toggle").empty() == toolsEnabled)
            throw std::runtime_error("Tool bindings do not match enabled modules");

        const auto fontId = ctx.resources->requestFont(
            GraphicsConstants::ENGINE_RESOURCES + "/fonts/gravitasfont.font.json");
        const auto* font = ctx.resources->getFont(fontId);
        if (font == nullptr || font->atlasTexture == 0 || font->glyphs.empty())
            throw std::runtime_error("Engine font atlas failed to load through the texture manager");
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

        for (bool toolsEnabled : {false, true})
        {
            config.tools.enabled = toolsEnabled;
            GravitasEngine engine(config);
            engine.registerScene("headless_smoke",
                                 [toolsEnabled]()
                                 {
                                     return std::make_unique<HeadlessRuntimeSmokeScene>(toolsEnabled);
                                 });
            engine.setActiveScene("headless_smoke");
            engine.start();
        }
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
