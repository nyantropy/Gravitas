#pragma once

#include <string>

#include "UiTypes.h"

enum class UiStyleState : uint8_t
{
    Normal = 0,
    Hover,
    Pressed,
    Focused,
    Selected,
    Disabled,
    Active,
    Checked,
    Expanded,
    Error
};

struct UiStyle
{
    std::string styleClass;
    UiStyleState styleState = UiStyleState::Normal;
    bool useStyleState = false;
    bool useBackgroundColor = false;
    bool useForegroundColor = false;
    bool useOpacity = false;
    UiColor backgroundColor = {1.0f, 1.0f, 1.0f, 1.0f};
    UiColor foregroundColor = {1.0f, 1.0f, 1.0f, 1.0f};
    float   opacity         = 1.0f;

    bool operator==(const UiStyle&) const = default;
};

struct UiStateFlags
{
    bool visible      = true;
    bool enabled      = true;
    bool interactable = false;
    bool hovered      = false;
    bool focused      = false;
    bool pressed      = false;

    bool operator==(const UiStateFlags&) const = default;
};
