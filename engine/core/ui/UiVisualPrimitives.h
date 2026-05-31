#pragma once

#include <string>
#include <variant>
#include <vector>

#include "UiHandle.h"
#include "UiTypes.h"

struct UiRectPrimitive
{
    UiHandle source = UI_INVALID_HANDLE;
    UiRect   bounds;
    UiRect   clipRect;
    UiColor  color;
};

struct UiImagePrimitive
{
    UiHandle     source = UI_INVALID_HANDLE;
    UiRect       bounds;
    UiRect       clipRect;
    std::string  imageAsset;
    UiColor      tint;
    float        imageAspect = 1.0f;
};

struct UiTextPrimitive
{
    UiHandle     source = UI_INVALID_HANDLE;
    UiRect       bounds;
    UiRect       clipRect;
    std::string  text;
    std::string  fontAsset;
    UiColor      color;
    float        scale = 1.0f;
    UiTextWrapMode wrapMode = UiTextWrapMode::None;
    UiHorizontalAlign horizontalAlign = UiHorizontalAlign::Left;
    UiVerticalAlign verticalAlign = UiVerticalAlign::Top;
    int          maxLines = 0;
};

struct UiLinePrimitive
{
    UiHandle source = UI_INVALID_HANDLE;
    UiVec2   start;
    UiVec2   end;
    UiRect   clipRect;
    UiColor  color;
    float    thickness = 0.002f;
};

using UiVisualPrimitive = std::variant<UiRectPrimitive, UiImagePrimitive, UiTextPrimitive, UiLinePrimitive>;

struct UiVisualList
{
    std::vector<UiVisualPrimitive> primitives;
};
