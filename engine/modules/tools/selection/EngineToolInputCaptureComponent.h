#pragma once

namespace gts::tools
{
    struct EngineToolInputCaptureComponent
    {
        bool pointerOverToolUi = false;
        bool toolUiPressed = false;
        bool keyboardCaptured = false;
        bool worldConsumed = false;
        bool primaryDown = false;
        bool primaryPressed = false;
        bool primaryReleased = false;
        bool orbitDown = false;
        bool orbitPressed = false;
        bool orbitReleased = false;
        bool panDown = false;
        bool panPressed = false;
        bool panReleased = false;
        bool pointerOverViewport = false;
        float pointerPixelX = 0.0f;
        float pointerPixelY = 0.0f;
        float pointerDeltaX = 0.0f;
        float pointerDeltaY = 0.0f;
        float viewportPointerX = 0.0f;
        float viewportPointerY = 0.0f;
        float scrollY = 0.0f;
    };
}
