#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

#include "EngineConfig.h"
#include "GravitasEngine.hpp"

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
            ctx.engineCommands->requestScreenshot(outputDirectory.string());
            ctx.engineCommands->requestQuit();
        }
    }

private:
    std::filesystem::path outputDirectory;
    bool requested = false;
};

int main()
{
    namespace fs = std::filesystem;

    const fs::path outputDirectory =
        fs::temp_directory_path() / "gts_headless_screenshot_runtime_smoke";
    fs::remove_all(outputDirectory);
    fs::create_directories(outputDirectory);

    try
    {
        EngineConfig config;
        config.graphics.backend = GraphicsBackend::Vulkan;
        config.graphics.headless = true;
        config.graphics.enableValidationLayers = false;
        config.graphics.renderWidth = 64;
        config.graphics.renderHeight = 64;
        config.graphics.maxFrameRate = 0;
        config.graphics.maxScreenshotsPerRun = 1;
        config.graphics.minSecondsBetweenScreenshots = 0.0f;
        config.simulationTickRate = 60;
        config.debugOverlayEnabledByDefault = false;

        GravitasEngine engine(config);
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

    for (const fs::directory_entry& entry : fs::directory_iterator(outputDirectory))
    {
        if (entry.path().extension() == ".png" && entry.file_size() > 0)
            return EXIT_SUCCESS;
    }

    std::cerr << "Headless screenshot smoke failed: no PNG was written to "
              << outputDirectory << std::endl;
    return EXIT_FAILURE;
}
