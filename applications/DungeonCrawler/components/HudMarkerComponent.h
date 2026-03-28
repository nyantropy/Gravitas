#pragma once

#include "UiHandle.h"
#include "BitmapFont.h"

// Tags a UiTree text element so HudSystem knows which field to update.
// Stores the layout parameters used when calling ctx.ui->update each frame.
struct HudMarkerComponent
{
    enum class Type { Health, Status, Message };
    Type        type    = Type::Health;
    UiHandle    uiHandle = UI_INVALID_HANDLE;
    BitmapFont* font    = nullptr;
    float       x       = 0.0f;
    float       y       = 0.0f;
    float       scale   = 0.035f;
};
