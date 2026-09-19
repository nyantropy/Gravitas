#pragma once

#include <unordered_map>
#include <vector>

#include "UiAnimationTypes.h"
#include "UiDocument.h"
#include "UiTheme.h"

struct UiAnimation
{
    UiAnimationId id = UI_INVALID_ANIMATION;
    UiHandle target = UI_INVALID_HANDLE;
    UiAnimationProperty property = UiAnimationProperty::Opacity;
    UiAnimationValue from = 0.0f;
    UiAnimationValue to = 0.0f;
    UiAnimationTiming timing;
    std::function<void(UiAnimationId, bool)> onFinished;
    gts::tween::Tween<float> progress;
    float delayRemaining = 0.0f;
    uint32_t completedRuns = 0;
    bool reversed = false;
};

class UiAnimationManager
{
public:
    void clear();

    UiAnimationId animate(UiDocument& document, UiAnimationDesc desc);
    UiAnimationId animateOpacity(UiDocument& document,
                                 UiHandle target,
                                 float opacity,
                                 const UiAnimationTiming& timing = {});
    std::vector<UiAnimationId> transitionStyleState(UiDocument& document,
                                                    const UiTheme& theme,
                                                    UiHandle target,
                                                    UiStyleState state,
                                                    const UiStyleTransitionDesc& desc = {});

    bool cancel(UiDocument& document, UiAnimationId animationId, bool complete = false);
    uint32_t cancel(UiDocument& document, UiHandle target, bool complete = false);
    uint32_t cancel(UiDocument& document,
                    UiHandle target,
                    UiAnimationProperty property,
                    bool complete = false);
    uint32_t cancelSubtree(UiDocument& document, UiHandle root, bool complete = false);
    void pruneInvalidNodes(UiDocument& document);

    UiAnimationFrameResult update(UiDocument& document, float dt);

    bool active() const { return !animations.empty(); }
    bool isAnimating(UiHandle target) const;
    bool isAnimating(UiHandle target, UiAnimationProperty property) const;
    size_t activeCount() const { return animations.size(); }
    const UiAnimation* find(UiAnimationId animationId) const;

private:
    UiAnimationId nextId();
    UiAnimationValue currentValue(const UiDocument& document,
                                  UiHandle target,
                                  UiAnimationProperty property) const;
    bool applyValue(UiDocument& document,
                    UiHandle target,
                    UiAnimationProperty property,
                    const UiAnimationValue& value) const;
    UiAnimationValue interpolateValue(const UiAnimationValue& from,
                                      const UiAnimationValue& to,
                                      float t) const;
    bool sameValueType(const UiAnimationValue& lhs, const UiAnimationValue& rhs) const;
    void startRun(UiAnimation& animation);
    bool finish(UiDocument& document, UiAnimationId animationId, bool completed, bool completeToEnd);
    void eraseAnimation(UiAnimationId animationId);
    uint64_t propertyKey(UiHandle target, UiAnimationProperty property) const;
    UiColor resolvedBackgroundColor(const UiComputedStyle& style,
                                    const UiAnimationValue& fallback) const;
    UiColor resolvedForegroundColor(const UiComputedStyle& style,
                                    const UiAnimationValue& fallback) const;

    std::unordered_map<UiAnimationId, UiAnimation> animations;
    std::unordered_map<uint64_t, UiAnimationId> propertyAnimations;
    UiAnimationId nextAnimationId = UI_INVALID_ANIMATION + 1;
};
