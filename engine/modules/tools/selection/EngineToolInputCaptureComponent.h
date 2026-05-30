#pragma once

#include "UiHandle.h"

namespace gts::tools
{
    struct EngineToolInputCaptureComponent
    {
        bool pointerOverToolUi = false;
        bool toolUiPressed = false;
        bool worldConsumed = false;
        bool primaryDown = false;
        bool primaryPressed = false;
        bool primaryReleased = false;
        float pointerX = 0.0f;
        float pointerY = 0.0f;
        UiHandle hoveredUi = UI_INVALID_HANDLE;
        UiHandle pressedUi = UI_INVALID_HANDLE;
    };
}
