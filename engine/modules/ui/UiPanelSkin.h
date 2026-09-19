#pragma once

#include <optional>
#include <string>

#include "UiStyle.h"

enum class UiPanelSkinType
{
    SolidColor = 0,
    Image,
    NineSlice
};

struct UiPanelSkin
{
    UiPanelSkinType type = UiPanelSkinType::SolidColor;
    std::string imageAsset;
    UiColor color = {0.030f, 0.036f, 0.048f, 0.88f};
    UiColor tint = {1.0f, 1.0f, 1.0f, 1.0f};
    UiThickness slice;
    UiThickness contentPadding;

    bool operator==(const UiPanelSkin&) const = default;
};

struct UiPanelStateSkin
{
    UiPanelSkin normal;
    std::optional<UiPanelSkin> hover;
    std::optional<UiPanelSkin> pressed;
    std::optional<UiPanelSkin> disabled;

    bool operator==(const UiPanelStateSkin&) const = default;
};

inline const UiPanelSkin& resolvePanelSkin(const UiPanelStateSkin& skin, const UiStateFlags& state)
{
    if (!state.enabled && skin.disabled.has_value())
        return *skin.disabled;

    if (state.pressed && skin.pressed.has_value())
        return *skin.pressed;

    if (state.hovered && skin.hover.has_value())
        return *skin.hover;

    return skin.normal;
}
