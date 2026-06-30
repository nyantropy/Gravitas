#pragma once

#include <cstdint>
#include <string>

#include "UiHandle.h"

using UiLayerId = uint32_t;

static constexpr UiLayerId UI_INVALID_LAYER = 0;
static constexpr UiLayerId UI_DEFAULT_LAYER = 1;

struct UiLayerState
{
    bool visible = true;
    bool inputEnabled = true;

    bool operator==(const UiLayerState&) const = default;
};

struct UiLayer
{
    UiLayerId id = UI_INVALID_LAYER;
    std::string name;
    int order = 0;
    UiLayerState state;
    UiHandle root = UI_INVALID_HANDLE;
};
