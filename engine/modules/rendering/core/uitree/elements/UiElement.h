#pragma once

#include "UiHandle.h"

enum class UiElementType { Image, Text };

struct UiElement
{
    UiHandle      handle  = UI_INVALID_HANDLE;
    UiElementType type    = UiElementType::Image;
    bool          visible = true;
};
