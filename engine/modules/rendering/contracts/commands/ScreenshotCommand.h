#pragma once

#include <string>
#include <utility>

#include "GtsCommandBuffer.h"

namespace gts::rendering
{
    inline constexpr const char* REQUEST_SCREENSHOT_COMMAND = "gts.rendering.request_screenshot";

    struct ScreenshotCommand
    {
        std::string directory;
    };

    // take a screenshot
    void requestScreenshot(GtsCommandBuffer& commands, std::string directory = {});
}
