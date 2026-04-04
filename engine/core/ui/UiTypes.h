#pragma once

#include <cstdint>

struct UiVec2
{
    float x = 0.0f;
    float y = 0.0f;

    bool operator==(const UiVec2&) const = default;
};

struct UiColor
{
    float r = 1.0f;
    float g = 1.0f;
    float b = 1.0f;
    float a = 1.0f;

    bool operator==(const UiColor&) const = default;
};

struct UiRect
{
    float x      = 0.0f;
    float y      = 0.0f;
    float width  = 0.0f;
    float height = 0.0f;

    bool operator==(const UiRect&) const = default;
};

struct UiThickness
{
    float left   = 0.0f;
    float top    = 0.0f;
    float right  = 0.0f;
    float bottom = 0.0f;

    bool operator==(const UiThickness&) const = default;
};

enum class UiNodeType : uint8_t
{
    Container = 0,
    Rect,
    Image,
    Text,
    Grid
};

enum class UiPositionMode : uint8_t
{
    Absolute = 0,
    Anchored
};

enum class UiSizeMode : uint8_t
{
    FromAnchors = 0,
    Fixed
};

enum class UiClipMode : uint8_t
{
    None = 0,
    ClipChildren
};

enum class UiDirtyFlags : uint8_t
{
    None      = 0,
    Structure = 1 << 0,
    Layout    = 1 << 1,
    Visual    = 1 << 2
};

inline UiDirtyFlags operator|(UiDirtyFlags lhs, UiDirtyFlags rhs)
{
    return static_cast<UiDirtyFlags>(static_cast<uint8_t>(lhs) | static_cast<uint8_t>(rhs));
}

inline UiDirtyFlags& operator|=(UiDirtyFlags& lhs, UiDirtyFlags rhs)
{
    lhs = lhs | rhs;
    return lhs;
}

inline bool hasFlag(UiDirtyFlags value, UiDirtyFlags flag)
{
    return (static_cast<uint8_t>(value) & static_cast<uint8_t>(flag)) != 0;
}
