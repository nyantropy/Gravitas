#include "ScreenshotCommand.h"
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
    require(commands.commands.size() == 5 && std::holds_alternative<GtsTogglePauseCommand>(commands.commands[0]) &&
            std::holds_alternative<GtsQuitCommand>(commands.commands[2]), "Original interleaved queue order");
    const auto& empty = std::get<GtsExtensionCommand>(commands.commands[1]);
    const auto& custom = std::get<GtsExtensionCommand>(commands.commands[3]);
    require(empty.name == gts::rendering::REQUEST_SCREENSHOT_COMMAND && custom.name == empty.name, "Owned extension identity");
    require(std::any_cast<const gts::rendering::ScreenshotCommand&>(empty.payload).directory.empty(), "Default directory unchanged");
    require(std::any_cast<const gts::rendering::ScreenshotCommand&>(custom.payload).directory == "captures/with spaces",
            "Directory copied without normalization or lifetime dependence");
}
