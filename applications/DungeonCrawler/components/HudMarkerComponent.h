#pragma once

#include "UiHandle.h"

// Tags a retained UI text node so HudSystem knows which field to update.
struct HudMarkerComponent
{
    enum class Type { Health, Status, Message };
    Type     type     = Type::Health;
    UiHandle uiHandle = UI_INVALID_HANDLE;
    float    scale    = 0.035f;
};
