#pragma once

#include <cstdint>

#include "UiTypes.h"

enum class UiLayoutMode : uint8_t
{
    Canvas = 0,
    Stack,
    Grid,
    Dock,
    Overlay,
    Scroll,
    Aspect,
    Constraint
};

enum class UiLayoutAxis : uint8_t
{
    Horizontal = 0,
    Vertical
};

enum class UiLayoutAlignment : uint8_t
{
    Start = 0,
    Center,
    End,
    Stretch
};

enum class UiDockEdge : uint8_t
{
    Left = 0,
    Right,
    Top,
    Bottom,
    Fill
};

enum class UiLayoutUnit : uint8_t
{
    Auto = 0,
    Normalized,
    Percent,
    SurfaceWidth,
    SurfaceHeight,
    ParentWidth,
    ParentHeight,
    Content,
    Em,
    Pixels
};

struct UiLayoutLength
{
    UiLayoutUnit unit = UiLayoutUnit::Auto;
    float value = 0.0f;

    bool operator==(const UiLayoutLength&) const = default;
};

struct UiLayoutConstraints
{
    UiLayoutLength minWidth;
    UiLayoutLength minHeight;
    UiLayoutLength maxWidth;
    UiLayoutLength maxHeight;
    UiLayoutLength preferredWidth;
    UiLayoutLength preferredHeight;
    float grow = 0.0f;
    float shrink = 1.0f;
    float aspectRatio = 0.0f;
    UiLayoutAlignment horizontalAlignment = UiLayoutAlignment::Stretch;
    UiLayoutAlignment verticalAlignment = UiLayoutAlignment::Stretch;

    bool operator==(const UiLayoutConstraints&) const = default;
};

struct UiLayoutSpec
{
    UiLayoutMode layoutMode = UiLayoutMode::Canvas;
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
    UiVec2      contentOffset = {0.0f, 0.0f};
    float       gap = 0.0f;

    UiLayoutAxis      stackAxis = UiLayoutAxis::Vertical;
    UiLayoutAlignment mainAxisAlignment = UiLayoutAlignment::Start;
    UiLayoutAlignment crossAxisAlignment = UiLayoutAlignment::Stretch;

    int gridColumns = 1;
    int gridRows = 1;
    float gridColumnGap = 0.0f;
    float gridRowGap = 0.0f;
    int gridColumn = 0;
    int gridRow = 0;
    int gridColumnSpan = 1;
    int gridRowSpan = 1;

    UiDockEdge dock = UiDockEdge::Fill;
    UiLayoutConstraints constraints;

    bool operator==(const UiLayoutSpec&) const = default;
};

struct UiComputedLayout
{
    UiRect bounds;
    UiRect contentRect;
    UiRect clipRect;
    UiVec2 measuredSize;

    bool operator==(const UiComputedLayout&) const = default;
};
