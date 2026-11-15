#include "ScreenshotCommand.h"

#include <utility>

namespace gts::rendering
{
    void requestScreenshot(GtsCommandBuffer& commands, std::string directory)
    {
        commands.requestExtensionCommand(REQUEST_SCREENSHOT_COMMAND, ScreenshotCommand{std::move(directory)});
    }
} // namespace gts::rendering
