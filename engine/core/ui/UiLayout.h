#pragma once

#include "UiTypes.h"

struct UiLayoutSpec
{
    UiPositionMode positionMode = UiPositionMode::Absolute;
    UiSizeMode     widthMode    = UiSizeMode::Fixed;
    UiSizeMode     heightMode   = UiSizeMode::Fixed;

    UiVec2 anchorMin = {0.0f, 0.0f};
    UiVec2 anchorMax = {0.0f, 0.0f};

    UiVec2 offsetMin = {0.0f, 0.0f};
    UiVec2 offsetMax = {0.0f, 0.0f};

    float fixedWidth  = 0.0f;
    float fixedHeight = 0.0f;

    UiThickness margin;
    UiThickness padding;
    UiClipMode  clipMode = UiClipMode::None;

    bool operator==(const UiLayoutSpec&) const = default;
};

struct UiComputedLayout
{
    UiRect bounds;
    UiRect contentRect;
    UiRect clipRect;

    bool operator==(const UiComputedLayout&) const = default;
};
