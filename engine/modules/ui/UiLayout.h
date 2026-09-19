#pragma once

#include <array>
#include "types/EnumName.h"

#include <cstdint>

#include "UiTypes.h"

enum class UiLayoutMode : uint8_t
{
    // Compatibility and low-level staging path. New feature UI should prefer
    // the structured containers below unless it is preserving old authored
    // geometry or drawing primitive visualizations.
    Canvas = 0,
    // Preferred authoring containers for ordinary UI structure.
    Stack,
    Grid,
    Dock,
    Overlay,
    Scroll,
    Aspect,
    Constraint
};

inline constexpr std::array uiLayoutModeNames{
    gts::EnumName{UiLayoutMode::Canvas, "Canvas"},
    gts::EnumName{UiLayoutMode::Stack, "Stack"},
    gts::EnumName{UiLayoutMode::Grid, "Grid"},
    gts::EnumName{UiLayoutMode::Dock, "Dock"},
    gts::EnumName{UiLayoutMode::Overlay, "Overlay"},
    gts::EnumName{UiLayoutMode::Scroll, "Scroll"},
    gts::EnumName{UiLayoutMode::Aspect, "Aspect"},
    gts::EnumName{UiLayoutMode::Constraint, "Constraint"}
};

enum class UiLayoutAxis : uint8_t
{
    Horizontal = 0,
    Vertical
};

inline constexpr std::array uiLayoutAxisNames{
    gts::EnumName{UiLayoutAxis::Horizontal, "Horizontal"},
    gts::EnumName{UiLayoutAxis::Vertical, "Vertical"}
};

enum class UiLayoutAlignment : uint8_t
{
    Start = 0,
    Center,
    End,
    Stretch
};

inline constexpr std::array uiLayoutAlignmentNames{
    gts::EnumName{UiLayoutAlignment::Start, "Start"},
    gts::EnumName{UiLayoutAlignment::Center, "Center"},
    gts::EnumName{UiLayoutAlignment::End, "End"},
    gts::EnumName{UiLayoutAlignment::Stretch, "Stretch"}
};

enum class UiDockEdge : uint8_t
{
    Left = 0,
    Right,
    Top,
    Bottom,
    Fill
};

inline constexpr std::array uiDockEdgeNames{
    gts::EnumName{UiDockEdge::Left, "Left"},
    gts::EnumName{UiDockEdge::Right, "Right"},
    gts::EnumName{UiDockEdge::Top, "Top"},
    gts::EnumName{UiDockEdge::Bottom, "Bottom"},
    gts::EnumName{UiDockEdge::Fill, "Fill"}
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

inline constexpr std::array uiLayoutUnitNames{
    gts::EnumName{UiLayoutUnit::Auto, "Auto"},
    gts::EnumName{UiLayoutUnit::Normalized, "Normalized"},
    gts::EnumName{UiLayoutUnit::Percent, "Percent"},
    gts::EnumName{UiLayoutUnit::SurfaceWidth, "SurfaceWidth"},
    gts::EnumName{UiLayoutUnit::SurfaceHeight, "SurfaceHeight"},
    gts::EnumName{UiLayoutUnit::ParentWidth, "ParentWidth"},
    gts::EnumName{UiLayoutUnit::ParentHeight, "ParentHeight"},
    gts::EnumName{UiLayoutUnit::Content, "Content"},
    gts::EnumName{UiLayoutUnit::Em, "Em"},
    gts::EnumName{UiLayoutUnit::Pixels, "Pixels"}
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
