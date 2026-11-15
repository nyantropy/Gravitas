#include "GtsCommandConsumer.h"
#include "GtsCommandBuffer.h"
#include "ScreenshotCommand.h"
#include "RenderEngineCommands.h"
#include <stdexcept>

#if __has_include("RenderingRuntime.h") || __has_include("IGtsGraphicsModule.hpp") || __has_include("GtsPlatform.h")
#error "Screenshot requests require only rendering command contracts and core transport"
#endif

void require(bool value, const char* message)
{
    if (!value) throw std::runtime_error(message);
}

int main()
{
    GtsCommandBuffer commands;
    commands.requestTogglePause();
    gts::rendering::requestScreenshot(commands);
    commands.requestQuit();
    std::string directory = "captures/with spaces";
    gts::rendering::requestScreenshot(commands, directory);
    directory = "changed";
    commands.requestExtensionCommand("other", 17);
    const auto pending = GtsCommandConsumer::takePendingCommands(commands);
    require(pending.size() == 5 && std::holds_alternative<GtsTogglePauseCommand>(pending[0]) &&
            std::holds_alternative<GtsQuitCommand>(pending[2]), "Original interleaved queue order");
    const auto& empty = std::get<GtsExtensionCommand>(pending[1]);
    const auto& custom = std::get<GtsExtensionCommand>(pending[3]);
    require(empty.name == gts::rendering::REQUEST_SCREENSHOT_COMMAND && custom.name == empty.name, "Owned extension identity");
    require(std::any_cast<const gts::rendering::ScreenshotCommand&>(empty.payload).directory.empty(), "Default directory unchanged");
    require(std::any_cast<const gts::rendering::ScreenshotCommand&>(custom.payload).directory == "captures/with spaces",
            "Directory copied without normalization or lifetime dependence");
    GraphicsSettings settings;
    settings.window.width = 777;
    gts::rendering::requestSetFrustumCullingEnabled(commands, false);
    gts::rendering::requestSetFrustumFreeze(commands, true);
    gts::rendering::requestApplyGraphicsSettings(commands, settings);
    settings.window.width = 888;
    const auto rendering = GtsCommandConsumer::takePendingCommands(commands);
    require(rendering.size() == 3, "Thin rendering contract preserves typed requests");
    require(!std::any_cast<const gts::rendering::SetFrustumCullingEnabledCommand&>(
                std::get<GtsExtensionCommand>(rendering[0]).payload).enabled
            && std::any_cast<const gts::rendering::SetFrustumFreezeCommand&>(
                std::get<GtsExtensionCommand>(rendering[1]).payload).frozen,
            "Typed frustum requests retain their own values/order");
    require(std::any_cast<const gts::rendering::ApplyGraphicsSettingsCommand&>(
                std::get<GtsExtensionCommand>(rendering[2]).payload).settings.window.width == 777,
            "Typed graphics request owns a copy of caller preferences");
}
