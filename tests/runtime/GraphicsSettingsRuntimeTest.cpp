#include <stdexcept>
#include <string>

#include "VulkanGraphics.hpp"

void require(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}

int main(int argc, char** argv) try
{
    const bool windowed = argc > 1 && std::string(argv[1]) == "--windowed";
    GraphicsConfig config;
    config.startup.headless = !windowed;
    config.startup.enableValidationLayers = false;
    config.settings.window.width = 320;
    config.settings.window.height = 240;
    config.settings.rendering.resolution.mode = RenderResolutionMode::Fixed;
    config.settings.rendering.resolution.fixedExtent = {160, 120};
    VulkanGraphics graphics(config);
    try
    {
        const auto initial = graphics.getGraphicsRuntimeState();
        require(initial.renderExtent == Extent2D{160, 120}, "Initial render extent differs");
        auto settings = graphics.getRequestedGraphicsSettings();
        settings.window.width = -1;
        require(!graphics.applyGraphicsSettings(settings).accepted(), "Invalid request accepted");
        require(graphics.getRequestedGraphicsSettings().window.width == 320, "Rejected request mutated preferences");
        settings = graphics.getRequestedGraphicsSettings();
        settings.framePacing.maxFrameRate = 60;
        require(graphics.applyGraphicsSettings(settings).accepted(), "Frame pacing change rejected");
        require(graphics.getGraphicsRuntimeState().renderExtent == initial.renderExtent, "Frame pacing resized renderer");
        settings.rendering.resolution.fixedExtent = {80, 60};
        const auto result = graphics.applyGraphicsSettings(settings);
        if (windowed)
        {
            require(result.status == GraphicsSettingsApplyStatus::Applied, "Fixed resolution change failed");
            require(graphics.getGraphicsRuntimeState().renderExtent == Extent2D{80, 60}, "Windowed fixed resolution ignored");
            settings.rendering.resolution.mode = RenderResolutionMode::ScaleOutput;
            settings.rendering.resolution.scale = 0.5f;
            require(graphics.applyGraphicsSettings(settings).accepted(), "Scaled resolution rejected");
            auto state = graphics.getGraphicsRuntimeState();
            require(resolveRenderExtent(settings.rendering.resolution, state.outputExtent) == state.renderExtent, "Scaled extent differs");
            settings.rendering.resolution.mode = RenderResolutionMode::MatchOutput;
            require(graphics.applyGraphicsSettings(settings).accepted(), "MatchOutput rejected");
            state = graphics.getGraphicsRuntimeState();
            require(state.renderExtent == state.outputExtent, "MatchOutput extent differs");
            require(state.presentMode.has_value(), "Missing effective presentation mode");
        }
        else
        {
            require(!result.accepted(), "Headless resize was silently accepted");
            require(!graphics.getGraphicsRuntimeState().presentMode.has_value(), "Headless state claims presentation");
        }
        graphics.shutdown();
    }
    catch (...)
    {
        graphics.shutdown();
        throw;
    }
}
catch (const std::runtime_error& error)
{
    if (std::string(error.what()).find("failed to find GPUs with Vulkan support") != std::string::npos)
        return 77;
    throw;
}
