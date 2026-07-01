#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <variant>

#include "Tween.h"
#include "UiHandle.h"
#include "UiStyle.h"
#include "UiTypes.h"

using UiAnimationId = uint64_t;

static constexpr UiAnimationId UI_INVALID_ANIMATION = 0;

enum class UiAnimationProperty : uint8_t
{
    Opacity = 0,
    BackgroundColor,
    ForegroundColor,
    RectColor,
    TextColor,
    ImageTint,
    NineSliceTint,
    LayoutOffsetMin,
    LayoutOffsetMax,
    LayoutFixedSize,
    LayoutContentOffset
};

using UiAnimationValue = std::variant<float, UiVec2, UiColor>;

struct UiAnimationTiming
{
    float durationSeconds = 0.12f;
    float delaySeconds = 0.0f;
    gts::tween::TweenEase ease = gts::tween::TweenEase::SmoothStep;
    uint32_t repeatCount = 0;
    bool loop = false;
    bool pingPong = false;
    bool snapToEnd = false;
};

struct UiAnimationDesc
{
    UiHandle target = UI_INVALID_HANDLE;
    UiAnimationProperty property = UiAnimationProperty::Opacity;
    UiAnimationValue to = 1.0f;
    std::optional<UiAnimationValue> from;
    UiAnimationTiming timing;
    std::function<void(UiAnimationId, bool)> onFinished;
};

struct UiStyleTransitionDesc
{
    UiAnimationTiming timing;
    std::optional<UiStyleState> fromState;
    bool animateBackground = true;
    bool animateForeground = true;
    bool animateOpacity = true;
};

struct UiAnimationFrameResult
{
    uint32_t active = 0;
    uint32_t updated = 0;
    uint32_t completed = 0;
    uint32_t cancelled = 0;

    bool changed() const
    {
        return updated > 0 || completed > 0 || cancelled > 0;
    }
};
