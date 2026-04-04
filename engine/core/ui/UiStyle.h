#pragma once

#include "UiTypes.h"

struct UiStyle
{
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
