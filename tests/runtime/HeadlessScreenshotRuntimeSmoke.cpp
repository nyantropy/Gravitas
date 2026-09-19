#include "ScreenshotCommand.h"
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

#include "EngineConfig.h"
#include "GravitasEngine.hpp"
#include "RenderEngineCommands.h"

class HeadlessScreenshotSmokeScene : public GtsScene
{
public:
    explicit HeadlessScreenshotSmokeScene(std::filesystem::path outputDirectory)
        : outputDirectory(std::move(outputDirectory))
    {
    }

    void onLoad(EcsControllerContext&, const GtsSceneTransitionData*) override
    {
    }

    void onUpdateSimulation(const EcsSimulationContext&) override
    {
    }

    void onUpdateControllers(const EcsControllerContext& ctx) override
    {
        if (requested)
            return;

        requested = true;
        if (ctx.engineCommands != nullptr)
        {
            gts::rendering::requestScreenshot(*ctx.engineCommands, (outputDirectory / "superseded").string());
            ctx.engineCommands->requestQuit();
            gts::rendering::requestSetFrustumFreeze(*ctx.engineCommands, false);
            ctx.engineCommands->requestExtensionCommand("test.unknown", 42);
            ctx.engineCommands->requestTogglePause();
            gts::rendering::requestScreenshot(*ctx.engineCommands, outputDirectory.string());
        }
    }

private:
    std::filesystem::path outputDirectory;
    bool requested = false;
};

int main(int argc, char** argv)
{
    namespace fs = std::filesystem;
    const bool automated = argc > 1 && std::string(argv[1]) == "--automation";

    const fs::path outputDirectory =
        fs::temp_directory_path() / (automated ? "gts_headless_screenshot_automation_smoke" : "gts_headless_screenshot_runtime_smoke");
    fs::remove_all(outputDirectory);
    fs::create_directories(outputDirectory);

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
        config.graphics.screenshots.maxScreenshotsPerRun = 1;
        config.graphics.screenshots.minSecondsBetweenScreenshots = 0.0f;
        config.simulation.tickRate = 60;
        config.tools.debugOverlayEnabledByDefault = false;

        GravitasEngine engine(config);
        if (automated)
        {
            gts::tools::ToolLaunchPreset preset;
            preset.screenshots.enabled = true;
            preset.screenshots.count = 1;
            preset.screenshots.afterSeconds = 0;
            preset.screenshots.exitAfterCapture = true;
            preset.screenshots.directory = (outputDirectory / "automated").string();
            engine.applyToolLaunchPreset(preset);
        }
        engine.registerScene("headless_screenshot_smoke",
                             [outputDirectory]()
                             {
                                 return std::make_unique<HeadlessScreenshotSmokeScene>(outputDirectory);
                             });
        engine.setActiveScene("headless_screenshot_smoke");
        engine.start();
    }
    catch (const std::runtime_error& error)
    {
        const std::string message = error.what();
        if (message.find("failed to find GPUs with Vulkan support") != std::string::npos)
        {
            std::cout << "Skipping headless screenshot smoke: no Vulkan-capable GPU available." << std::endl;
            return 77;
        }

        throw;
    }

    const fs::path captureDirectory = automated ? outputDirectory / "automated" : outputDirectory;
    if (automated && fs::exists(outputDirectory / "screenshot_0000.png"))
        throw std::runtime_error("Automation must run after queued screenshot requests");
    for (const fs::directory_entry& entry : fs::directory_iterator(captureDirectory))
    {
        if (entry.path().extension() == ".png" && entry.file_size() > 0)
        {
            if (entry.path().filename() != "screenshot_0000.png" || fs::exists(outputDirectory / "superseded"))
                throw std::runtime_error("Screenshot requests must preserve last-request order and naming");
            return EXIT_SUCCESS;
        }
    }

    std::cerr << "Headless screenshot smoke failed: no PNG was written to "
              << outputDirectory << std::endl;
    return EXIT_FAILURE;
}
