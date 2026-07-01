#pragma once

#include <unordered_map>
#include <vector>

#include "UiAnimationManager.h"
#include "UiBindingTypes.h"
#include "UiDocument.h"

class UiAccessibilityManager;

struct UiBinding
{
    UiBindingId id = UI_INVALID_BINDING;
    UiBindingDesc desc;
    UiBindingValue lastValue;
    uint64_t lastRevision = 0;
    bool initialized = false;
};

class UiBindingManager
{
public:
    void clear();

    UiBindingId bind(UiDocument& document,
                     UiAnimationManager& animationManager,
                     UiBindingDesc desc,
                     UiAccessibilityManager* accessibilityManager = nullptr);
    bool unbind(UiBindingId bindingId);
    uint32_t unbindTarget(UiHandle target);
    uint32_t unbindSubtree(const UiDocument& document, UiHandle root);
    uint32_t unbindMount(UiMountId mountId);
    void pruneInvalidNodes(const UiDocument& document);

    UiBindingFrameResult update(UiDocument& document,
                                UiAnimationManager& animationManager,
                                UiAccessibilityManager* accessibilityManager = nullptr);

    const UiBinding* find(UiBindingId bindingId) const;
    size_t activeCount() const { return bindings.size(); }
    std::vector<UiBindingId> bindingsForTarget(UiHandle target) const;

private:
    UiBindingId nextId();
    bool sourceAlive(const UiBindingSource& source) const;
    bool shouldEvaluate(const UiBinding& binding, uint64_t revision) const;
    bool evaluateAndApply(UiBinding& binding,
                          UiDocument& document,
                          UiAnimationManager& animationManager,
                          UiAccessibilityManager* accessibilityManager,
                          bool initial);
    bool applyValue(UiBinding& binding,
                    UiDocument& document,
                    UiAnimationManager& animationManager,
                    const UiBindingValue& value,
                    bool initial) const;
    bool applyText(UiDocument& document,
                   UiHandle target,
                   const UiBindingValue& value,
                   const UiBindingFormatter& formatter) const;
    bool applyVisible(UiDocument& document, UiHandle target, bool visible) const;
    bool applyEnabled(UiDocument& document, UiHandle target, bool enabled) const;
    bool applyInteractable(UiDocument& document, UiHandle target, bool interactable) const;
    bool applyOpacity(UiBinding& binding,
                      UiDocument& document,
                      UiAnimationManager& animationManager,
                      float opacity,
                      bool initial) const;
    bool applyProgress(UiBinding& binding,
                       UiDocument& document,
                       UiAnimationManager& animationManager,
                       float progress,
                       bool initial) const;
    bool applyRectColor(UiBinding& binding,
                        UiDocument& document,
                        UiAnimationManager& animationManager,
                        const UiColor& color,
                        bool initial) const;
    bool applyTextColor(UiBinding& binding,
                        UiDocument& document,
                        UiAnimationManager& animationManager,
                        const UiColor& color,
                        bool initial) const;
    bool applyImageAsset(UiDocument& document, UiHandle target, const std::string& imageAsset) const;
    bool applyImageTint(UiBinding& binding,
                        UiDocument& document,
                        UiAnimationManager& animationManager,
                        const UiColor& tint,
                        bool initial) const;
    bool applyStyleClass(UiDocument& document, UiHandle target, const std::string& styleClass) const;
    bool applyLayout(UiDocument& document, UiHandle target, const UiLayoutSpec& layout) const;
    bool applyLayoutVec2(UiBinding& binding,
                         UiDocument& document,
                         UiAnimationManager& animationManager,
                         UiAnimationProperty property,
                         const UiVec2& value,
                         bool initial) const;
    bool animate(UiBinding& binding,
                 UiDocument& document,
                 UiAnimationManager& animationManager,
                 UiAnimationProperty property,
                 const UiAnimationValue& to,
                 bool initial) const;

    std::unordered_map<UiBindingId, UiBinding> bindings;
    UiBindingId nextBindingId = UI_INVALID_BINDING + 1;
};
