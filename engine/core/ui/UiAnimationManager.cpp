#include "UiAnimationManager.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include "UiNode.h"

namespace
{
    float clampOpacity(float value)
    {
        return std::clamp(value, 0.0f, 1.0f);
    }

    UiColor lerpColor(const UiColor& from, const UiColor& to, float t)
    {
        return {
            gts::tween::lerp(from.r, to.r, t),
            gts::tween::lerp(from.g, to.g, t),
            gts::tween::lerp(from.b, to.b, t),
            gts::tween::lerp(from.a, to.a, t)
        };
    }

    UiVec2 lerpVec2(const UiVec2& from, const UiVec2& to, float t)
    {
        return {
            gts::tween::lerp(from.x, to.x, t),
            gts::tween::lerp(from.y, to.y, t)
        };
    }
}

void UiAnimationManager::clear()
{
    animations.clear();
    propertyAnimations.clear();
    nextAnimationId = UI_INVALID_ANIMATION + 1;
}

UiAnimationId UiAnimationManager::animate(UiDocument& document, UiAnimationDesc desc)
{
    if (desc.target == UI_INVALID_HANDLE || document.findNode(desc.target) == nullptr)
        return UI_INVALID_ANIMATION;

    UiAnimationValue from = desc.from.value_or(currentValue(document, desc.target, desc.property));
    if (!sameValueType(from, desc.to))
        return UI_INVALID_ANIMATION;

    cancel(document, desc.target, desc.property, false);

    const UiAnimationId id = nextId();
    const float duration = std::max(0.0f, desc.timing.durationSeconds);
    const float delay = std::max(0.0f, desc.timing.delaySeconds);

    if (desc.timing.snapToEnd || (duration <= 0.0f && delay <= 0.0f))
    {
        applyValue(document, desc.target, desc.property, desc.to);
        if (desc.onFinished)
            desc.onFinished(id, true);
        return id;
    }

    UiAnimation animation;
    animation.id = id;
    animation.target = desc.target;
    animation.property = desc.property;
    animation.from = from;
    animation.to = desc.to;
    animation.timing = desc.timing;
    animation.timing.durationSeconds = duration;
    animation.timing.delaySeconds = delay;
    animation.onFinished = std::move(desc.onFinished);
    animation.delayRemaining = delay;
    startRun(animation);

    applyValue(document, animation.target, animation.property, animation.from);

    propertyAnimations[propertyKey(animation.target, animation.property)] = id;
    animations.emplace(id, std::move(animation));
    return id;
}

UiAnimationId UiAnimationManager::animateOpacity(UiDocument& document,
                                                UiHandle target,
                                                float opacity,
                                                const UiAnimationTiming& timing)
{
    UiAnimationDesc desc;
    desc.target = target;
    desc.property = UiAnimationProperty::Opacity;
    desc.to = clampOpacity(opacity);
    desc.timing = timing;
    return animate(document, std::move(desc));
}

std::vector<UiAnimationId> UiAnimationManager::transitionStyleState(UiDocument& document,
                                                                    const UiTheme& theme,
                                                                    UiHandle target,
                                                                    UiStyleState state,
                                                                    const UiStyleTransitionDesc& desc)
{
    std::vector<UiAnimationId> ids;
    UiNode* node = document.findNode(target);
    if (node == nullptr)
        return ids;

    UiComputedStyle actualStyle;
    theme.resolveNodeStyle(node->style, node->state, actualStyle);

    UiComputedStyle fromStateStyle = actualStyle;
    if (desc.fromState)
    {
        UiStyle fromStyle = node->style;
        fromStyle.useStyleState = true;
        fromStyle.styleState = *desc.fromState;
        fromStyle.useBackgroundColor = false;
        fromStyle.useForegroundColor = false;
        fromStyle.useOpacity = false;
        theme.resolveNodeStyle(fromStyle, node->state, fromStateStyle);
    }

    UiStyle targetStyle = node->style;
    targetStyle.useStyleState = true;
    targetStyle.styleState = state;
    targetStyle.useBackgroundColor = false;
    targetStyle.useForegroundColor = false;
    targetStyle.useOpacity = false;

    UiComputedStyle resolvedTargetStyle;
    theme.resolveNodeStyle(targetStyle, node->state, resolvedTargetStyle);
    document.setStyle(target, targetStyle);

    const UiComputedStyle& currentBackgroundStyle =
        (node->style.useBackgroundColor || isAnimating(target, UiAnimationProperty::BackgroundColor))
            ? actualStyle
            : fromStateStyle;
    const UiComputedStyle& currentForegroundStyle =
        (node->style.useForegroundColor || isAnimating(target, UiAnimationProperty::ForegroundColor))
            ? actualStyle
            : fromStateStyle;
    const UiComputedStyle& currentOpacityStyle =
        (node->style.useOpacity || isAnimating(target, UiAnimationProperty::Opacity))
            ? actualStyle
            : fromStateStyle;

    if (desc.animateBackground && (currentBackgroundStyle.hasBackgroundColor || currentBackgroundStyle.hasSkin ||
                                   resolvedTargetStyle.hasBackgroundColor || resolvedTargetStyle.hasSkin))
    {
        UiAnimationDesc animation;
        animation.target = target;
        animation.property = UiAnimationProperty::BackgroundColor;
        animation.from = resolvedBackgroundColor(currentBackgroundStyle,
                                                 currentValue(document,
                                                              target,
                                                              UiAnimationProperty::BackgroundColor));
        animation.to = resolvedBackgroundColor(resolvedTargetStyle, *animation.from);
        animation.timing = desc.timing;
        ids.push_back(animate(document, std::move(animation)));
    }

    if (desc.animateForeground && (currentForegroundStyle.hasForegroundColor || resolvedTargetStyle.hasForegroundColor))
    {
        UiAnimationDesc animation;
        animation.target = target;
        animation.property = UiAnimationProperty::ForegroundColor;
        animation.from = resolvedForegroundColor(currentForegroundStyle,
                                                 currentValue(document,
                                                              target,
                                                              UiAnimationProperty::ForegroundColor));
        animation.to = resolvedForegroundColor(resolvedTargetStyle, *animation.from);
        animation.timing = desc.timing;
        ids.push_back(animate(document, std::move(animation)));
    }

    if (desc.animateOpacity && (currentOpacityStyle.hasOpacity || resolvedTargetStyle.hasOpacity))
    {
        UiAnimationDesc animation;
        animation.target = target;
        animation.property = UiAnimationProperty::Opacity;
        animation.from = currentOpacityStyle.hasOpacity ? clampOpacity(currentOpacityStyle.opacity) : 1.0f;
        animation.to = resolvedTargetStyle.hasOpacity ? clampOpacity(resolvedTargetStyle.opacity) : 1.0f;
        animation.timing = desc.timing;
        ids.push_back(animate(document, std::move(animation)));
    }

    ids.erase(std::remove(ids.begin(), ids.end(), UI_INVALID_ANIMATION), ids.end());
    return ids;
}

bool UiAnimationManager::cancel(UiDocument& document, UiAnimationId animationId, bool complete)
{
    return finish(document, animationId, complete, complete);
}

uint32_t UiAnimationManager::cancel(UiDocument& document, UiHandle target, bool complete)
{
    std::vector<UiAnimationId> ids;
    for (const auto& [id, animation] : animations)
    {
        if (animation.target == target)
            ids.push_back(id);
    }

    uint32_t cancelled = 0;
    for (UiAnimationId id : ids)
    {
        if (cancel(document, id, complete))
            ++cancelled;
    }
    return cancelled;
}

uint32_t UiAnimationManager::cancel(UiDocument& document,
                                    UiHandle target,
                                    UiAnimationProperty property,
                                    bool complete)
{
    const auto it = propertyAnimations.find(propertyKey(target, property));
    if (it == propertyAnimations.end())
        return 0;

    return cancel(document, it->second, complete) ? 1u : 0u;
}

uint32_t UiAnimationManager::cancelSubtree(UiDocument& document, UiHandle root, bool complete)
{
    if (root == UI_INVALID_HANDLE)
        return 0;

    std::vector<UiAnimationId> ids;
    for (const auto& [id, animation] : animations)
    {
        if (animation.target == root || document.isDescendantOf(animation.target, root))
            ids.push_back(id);
    }

    uint32_t cancelled = 0;
    for (UiAnimationId id : ids)
    {
        if (cancel(document, id, complete))
            ++cancelled;
    }
    return cancelled;
}

void UiAnimationManager::pruneInvalidNodes(UiDocument& document)
{
    std::vector<UiAnimationId> ids;
    for (const auto& [id, animation] : animations)
    {
        if (document.findNode(animation.target) == nullptr)
            ids.push_back(id);
    }

    for (UiAnimationId id : ids)
        finish(document, id, false, false);
}

UiAnimationFrameResult UiAnimationManager::update(UiDocument& document, float dt)
{
    UiAnimationFrameResult result;
    if (animations.empty())
        return result;

    pruneInvalidNodes(document);
    const float step = std::max(0.0f, dt);

    std::vector<UiAnimationId> ids;
    ids.reserve(animations.size());
    for (const auto& [id, _] : animations)
        ids.push_back(id);
    std::sort(ids.begin(), ids.end());

    for (UiAnimationId id : ids)
    {
        auto it = animations.find(id);
        if (it == animations.end())
            continue;

        UiAnimation& animation = it->second;
        if (document.findNode(animation.target) == nullptr)
        {
            finish(document, id, false, false);
            ++result.cancelled;
            continue;
        }

        if (animation.delayRemaining > 0.0f)
        {
            animation.delayRemaining = std::max(0.0f, animation.delayRemaining - step);
            if (animation.delayRemaining > 0.0f)
                continue;
        }

        if (animation.timing.durationSeconds <= 0.0f)
        {
            applyValue(document,
                       animation.target,
                       animation.property,
                       animation.reversed ? animation.from : animation.to);
            finish(document, id, true, false);
            ++result.updated;
            ++result.completed;
            continue;
        }

        const bool runComplete = animation.progress.update(step);
        const float progress = animation.reversed
            ? 1.0f - animation.progress.value()
            : animation.progress.value();
        applyValue(document,
                   animation.target,
                   animation.property,
                   interpolateValue(animation.from, animation.to, progress));
        ++result.updated;

        if (!runComplete)
            continue;

        const bool repeats = animation.timing.loop ||
                             animation.completedRuns < animation.timing.repeatCount;
        if (repeats)
        {
            ++animation.completedRuns;
            if (animation.timing.pingPong)
                animation.reversed = !animation.reversed;
            startRun(animation);
            continue;
        }

        finish(document, id, true, false);
        ++result.completed;
    }

    result.active = static_cast<uint32_t>(animations.size());
    return result;
}

bool UiAnimationManager::isAnimating(UiHandle target) const
{
    for (const auto& [_, animation] : animations)
    {
        if (animation.target == target)
            return true;
    }
    return false;
}

bool UiAnimationManager::isAnimating(UiHandle target, UiAnimationProperty property) const
{
    return propertyAnimations.find(propertyKey(target, property)) != propertyAnimations.end();
}

const UiAnimation* UiAnimationManager::find(UiAnimationId animationId) const
{
    const auto it = animations.find(animationId);
    return it == animations.end() ? nullptr : &it->second;
}

UiAnimationId UiAnimationManager::nextId()
{
    return nextAnimationId++;
}

UiAnimationValue UiAnimationManager::currentValue(const UiDocument& document,
                                                  UiHandle target,
                                                  UiAnimationProperty property) const
{
    const UiNode* node = document.findNode(target);
    if (node == nullptr)
        return 0.0f;

    switch (property)
    {
        case UiAnimationProperty::Opacity:
            return node->style.useOpacity ? clampOpacity(node->style.opacity) : 1.0f;

        case UiAnimationProperty::BackgroundColor:
            if (node->style.useBackgroundColor)
                return node->style.backgroundColor;
            if (const auto* rect = std::get_if<UiRectData>(&node->payload))
                return rect->color;
            return UiColor{};

        case UiAnimationProperty::ForegroundColor:
            if (node->style.useForegroundColor)
                return node->style.foregroundColor;
            if (const auto* text = std::get_if<UiTextData>(&node->payload))
                return text->color;
            return UiColor{};

        case UiAnimationProperty::RectColor:
            if (const auto* rect = std::get_if<UiRectData>(&node->payload))
                return rect->color;
            return UiColor{};

        case UiAnimationProperty::TextColor:
            if (const auto* text = std::get_if<UiTextData>(&node->payload))
                return text->color;
            return UiColor{};

        case UiAnimationProperty::ImageTint:
            if (const auto* image = std::get_if<UiImageData>(&node->payload))
                return image->tint;
            return UiColor{};

        case UiAnimationProperty::NineSliceTint:
            if (const auto* slice = std::get_if<UiNineSliceData>(&node->payload))
                return slice->tint;
            return UiColor{};

        case UiAnimationProperty::LayoutOffsetMin:
            return node->layout.offsetMin;

        case UiAnimationProperty::LayoutOffsetMax:
            return node->layout.offsetMax;

        case UiAnimationProperty::LayoutAnchorMin:
            return node->layout.anchorMin;

        case UiAnimationProperty::LayoutAnchorMax:
            return node->layout.anchorMax;

        case UiAnimationProperty::LayoutFixedSize:
            return UiVec2{node->layout.fixedWidth, node->layout.fixedHeight};

        case UiAnimationProperty::LayoutContentOffset:
            return node->layout.contentOffset;
    }

    return 0.0f;
}

bool UiAnimationManager::applyValue(UiDocument& document,
                                    UiHandle target,
                                    UiAnimationProperty property,
                                    const UiAnimationValue& value) const
{
    UiNode* node = document.findNode(target);
    if (node == nullptr)
        return false;

    switch (property)
    {
        case UiAnimationProperty::Opacity:
        {
            const auto* opacity = std::get_if<float>(&value);
            if (opacity == nullptr)
                return false;
            UiStyle style = node->style;
            style.useOpacity = true;
            style.opacity = clampOpacity(*opacity);
            return document.setStyle(target, style);
        }

        case UiAnimationProperty::BackgroundColor:
        {
            const auto* color = std::get_if<UiColor>(&value);
            if (color == nullptr)
                return false;
            UiStyle style = node->style;
            style.useBackgroundColor = true;
            style.backgroundColor = *color;
            return document.setStyle(target, style);
        }

        case UiAnimationProperty::ForegroundColor:
        {
            const auto* color = std::get_if<UiColor>(&value);
            if (color == nullptr)
                return false;
            UiStyle style = node->style;
            style.useForegroundColor = true;
            style.foregroundColor = *color;
            return document.setStyle(target, style);
        }

        case UiAnimationProperty::RectColor:
        {
            const auto* color = std::get_if<UiColor>(&value);
            auto* rect = std::get_if<UiRectData>(&node->payload);
            if (color == nullptr || rect == nullptr)
                return false;
            UiRectData data = *rect;
            data.color = *color;
            return document.setPayload(target, data);
        }

        case UiAnimationProperty::TextColor:
        {
            const auto* color = std::get_if<UiColor>(&value);
            auto* text = std::get_if<UiTextData>(&node->payload);
            if (color == nullptr || text == nullptr)
                return false;
            UiTextData data = *text;
            data.color = *color;
            return document.setPayload(target, data);
        }

        case UiAnimationProperty::ImageTint:
        {
            const auto* color = std::get_if<UiColor>(&value);
            auto* image = std::get_if<UiImageData>(&node->payload);
            if (color == nullptr || image == nullptr)
                return false;
            UiImageData data = *image;
            data.tint = *color;
            return document.setPayload(target, data);
        }

        case UiAnimationProperty::NineSliceTint:
        {
            const auto* color = std::get_if<UiColor>(&value);
            auto* slice = std::get_if<UiNineSliceData>(&node->payload);
            if (color == nullptr || slice == nullptr)
                return false;
            UiNineSliceData data = *slice;
            data.tint = *color;
            return document.setPayload(target, data);
        }

        case UiAnimationProperty::LayoutOffsetMin:
        {
            const auto* vec = std::get_if<UiVec2>(&value);
            if (vec == nullptr)
                return false;
            UiLayoutSpec layout = node->layout;
            layout.offsetMin = *vec;
            return document.setLayout(target, layout);
        }

        case UiAnimationProperty::LayoutOffsetMax:
        {
            const auto* vec = std::get_if<UiVec2>(&value);
            if (vec == nullptr)
                return false;
            UiLayoutSpec layout = node->layout;
            layout.offsetMax = *vec;
            return document.setLayout(target, layout);
        }

        case UiAnimationProperty::LayoutAnchorMin:
        {
            const auto* vec = std::get_if<UiVec2>(&value);
            if (vec == nullptr)
                return false;
            UiLayoutSpec layout = node->layout;
            layout.positionMode = UiPositionMode::Anchored;
            layout.widthMode = UiSizeMode::FromAnchors;
            layout.heightMode = UiSizeMode::FromAnchors;
            layout.anchorMin = *vec;
            return document.setLayout(target, layout);
        }

        case UiAnimationProperty::LayoutAnchorMax:
        {
            const auto* vec = std::get_if<UiVec2>(&value);
            if (vec == nullptr)
                return false;
            UiLayoutSpec layout = node->layout;
            layout.positionMode = UiPositionMode::Anchored;
            layout.widthMode = UiSizeMode::FromAnchors;
            layout.heightMode = UiSizeMode::FromAnchors;
            layout.anchorMax = *vec;
            return document.setLayout(target, layout);
        }

        case UiAnimationProperty::LayoutFixedSize:
        {
            const auto* vec = std::get_if<UiVec2>(&value);
            if (vec == nullptr)
                return false;
            UiLayoutSpec layout = node->layout;
            layout.widthMode = UiSizeMode::Fixed;
            layout.heightMode = UiSizeMode::Fixed;
            layout.fixedWidth = std::max(0.0f, vec->x);
            layout.fixedHeight = std::max(0.0f, vec->y);
            return document.setLayout(target, layout);
        }

        case UiAnimationProperty::LayoutContentOffset:
        {
            const auto* vec = std::get_if<UiVec2>(&value);
            if (vec == nullptr)
                return false;
            UiLayoutSpec layout = node->layout;
            layout.contentOffset = *vec;
            return document.setLayout(target, layout);
        }
    }

    return false;
}

UiAnimationValue UiAnimationManager::interpolateValue(const UiAnimationValue& from,
                                                      const UiAnimationValue& to,
                                                      float t) const
{
    t = std::clamp(t, 0.0f, 1.0f);
    if (const auto* fromFloat = std::get_if<float>(&from))
        return gts::tween::lerp(*fromFloat, std::get<float>(to), t);
    if (const auto* fromVec = std::get_if<UiVec2>(&from))
        return lerpVec2(*fromVec, std::get<UiVec2>(to), t);
    if (const auto* fromColor = std::get_if<UiColor>(&from))
        return lerpColor(*fromColor, std::get<UiColor>(to), t);
    return to;
}

bool UiAnimationManager::sameValueType(const UiAnimationValue& lhs, const UiAnimationValue& rhs) const
{
    return lhs.index() == rhs.index();
}

void UiAnimationManager::startRun(UiAnimation& animation)
{
    animation.progress.start(0.0f,
                             1.0f,
                             std::max(0.0f, animation.timing.durationSeconds),
                             animation.timing.ease);
}

bool UiAnimationManager::finish(UiDocument& document,
                                UiAnimationId animationId,
                                bool completed,
                                bool completeToEnd)
{
    auto it = animations.find(animationId);
    if (it == animations.end())
        return false;

    UiAnimation animation = std::move(it->second);
    if (completeToEnd)
    {
        applyValue(document,
                   animation.target,
                   animation.property,
                   animation.reversed ? animation.from : animation.to);
    }

    eraseAnimation(animationId);
    if (animation.onFinished)
        animation.onFinished(animationId, completed);
    return true;
}

void UiAnimationManager::eraseAnimation(UiAnimationId animationId)
{
    auto it = animations.find(animationId);
    if (it == animations.end())
        return;

    propertyAnimations.erase(propertyKey(it->second.target, it->second.property));
    animations.erase(it);
}

uint64_t UiAnimationManager::propertyKey(UiHandle target, UiAnimationProperty property) const
{
    return (static_cast<uint64_t>(target) << 32u) |
           static_cast<uint64_t>(static_cast<uint8_t>(property));
}

UiColor UiAnimationManager::resolvedBackgroundColor(const UiComputedStyle& style,
                                                    const UiAnimationValue& fallback) const
{
    if (style.hasSkin && style.skin.type == UiPanelSkinType::SolidColor)
        return style.skin.color;
    if (style.hasBackgroundColor)
        return style.backgroundColor;
    if (const auto* color = std::get_if<UiColor>(&fallback))
        return *color;
    return UiColor{};
}

UiColor UiAnimationManager::resolvedForegroundColor(const UiComputedStyle& style,
                                                    const UiAnimationValue& fallback) const
{
    if (style.hasForegroundColor)
        return style.foregroundColor;
    if (const auto* color = std::get_if<UiColor>(&fallback))
        return *color;
    return UiColor{};
}
