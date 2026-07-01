#include "UiBindingManager.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <utility>

namespace
{
    float clamp01(float value)
    {
        return std::clamp(value, 0.0f, 1.0f);
    }

    std::string payloadToString(const UiNodePayload& payload)
    {
        if (const auto* text = std::get_if<UiTextData>(&payload))
            return text->text;
        if (const auto* image = std::get_if<UiImageData>(&payload))
            return image->imageAsset;
        if (const auto* slice = std::get_if<UiNineSliceData>(&payload))
            return slice->imageAsset;
        return {};
    }
}

UiBindingSource makeBindingSource(std::function<UiBindingValue()> read,
                                  std::function<uint64_t()> revision,
                                  std::function<bool()> alive)
{
    UiBindingSource source;
    source.read = std::move(read);
    source.revision = std::move(revision);
    source.alive = std::move(alive);
    return source;
}

bool uiBindingValueAsBool(const UiBindingValue& value, bool& outValue)
{
    if (const auto* typed = std::get_if<bool>(&value))
    {
        outValue = *typed;
        return true;
    }
    if (const auto* typed = std::get_if<int64_t>(&value))
    {
        outValue = *typed != 0;
        return true;
    }
    if (const auto* typed = std::get_if<double>(&value))
    {
        outValue = std::fabs(*typed) > 0.000001;
        return true;
    }
    if (const auto* typed = std::get_if<std::string>(&value))
    {
        outValue = !typed->empty();
        return true;
    }
    return false;
}

bool uiBindingValueAsFloat(const UiBindingValue& value, float& outValue)
{
    if (const auto* typed = std::get_if<bool>(&value))
    {
        outValue = *typed ? 1.0f : 0.0f;
        return true;
    }
    if (const auto* typed = std::get_if<int64_t>(&value))
    {
        outValue = static_cast<float>(*typed);
        return true;
    }
    if (const auto* typed = std::get_if<double>(&value))
    {
        outValue = static_cast<float>(*typed);
        return true;
    }
    if (const auto* typed = std::get_if<std::string>(&value))
    {
        char* end = nullptr;
        const float parsed = std::strtof(typed->c_str(), &end);
        if (end != typed->c_str())
        {
            outValue = parsed;
            return true;
        }
    }
    return false;
}

bool uiBindingValueAsString(const UiBindingValue& value, std::string& outValue)
{
    outValue = uiBindingValueToString(value);
    return true;
}

bool uiBindingValueAsColor(const UiBindingValue& value, UiColor& outValue)
{
    if (const auto* typed = std::get_if<UiColor>(&value))
    {
        outValue = *typed;
        return true;
    }
    return false;
}

bool uiBindingValueAsVec2(const UiBindingValue& value, UiVec2& outValue)
{
    if (const auto* typed = std::get_if<UiVec2>(&value))
    {
        outValue = *typed;
        return true;
    }
    return false;
}

std::string uiBindingValueToString(const UiBindingValue& value)
{
    if (std::holds_alternative<std::monostate>(value))
        return {};
    if (const auto* typed = std::get_if<bool>(&value))
        return *typed ? "true" : "false";
    if (const auto* typed = std::get_if<int64_t>(&value))
        return std::to_string(*typed);
    if (const auto* typed = std::get_if<double>(&value))
    {
        std::ostringstream out;
        out << *typed;
        return out.str();
    }
    if (const auto* typed = std::get_if<std::string>(&value))
        return *typed;
    if (const auto* typed = std::get_if<UiNodePayload>(&value))
        return payloadToString(*typed);
    return {};
}

void UiBindingManager::clear()
{
    bindings.clear();
    nextBindingId = UI_INVALID_BINDING + 1;
}

UiBindingId UiBindingManager::bind(UiDocument& document,
                                   UiAnimationManager& animationManager,
                                   UiBindingDesc desc)
{
    if (desc.target == UI_INVALID_HANDLE || desc.source.read == nullptr ||
        document.findNode(desc.target) == nullptr || !sourceAlive(desc.source))
    {
        return UI_INVALID_BINDING;
    }

    UiBinding binding;
    binding.id = nextId();
    binding.desc = std::move(desc);

    const UiBindingId bindingId = binding.id;
    bindings.emplace(bindingId, std::move(binding));

    if (bindings.at(bindingId).desc.applyImmediately)
    {
        if (!evaluateAndApply(bindings.at(bindingId), document, animationManager, true))
        {
            bindings.erase(bindingId);
            return UI_INVALID_BINDING;
        }
    }

    return bindingId;
}

bool UiBindingManager::unbind(UiBindingId bindingId)
{
    return bindings.erase(bindingId) > 0;
}

uint32_t UiBindingManager::unbindTarget(UiHandle target)
{
    uint32_t removed = 0;
    for (auto it = bindings.begin(); it != bindings.end();)
    {
        if (it->second.desc.target == target)
        {
            it = bindings.erase(it);
            ++removed;
        }
        else
        {
            ++it;
        }
    }
    return removed;
}

uint32_t UiBindingManager::unbindSubtree(const UiDocument& document, UiHandle root)
{
    if (root == UI_INVALID_HANDLE)
        return 0;

    uint32_t removed = 0;
    for (auto it = bindings.begin(); it != bindings.end();)
    {
        const UiHandle target = it->second.desc.target;
        if (target == root || document.isDescendantOf(target, root))
        {
            it = bindings.erase(it);
            ++removed;
        }
        else
        {
            ++it;
        }
    }
    return removed;
}

uint32_t UiBindingManager::unbindMount(UiMountId mountId)
{
    if (mountId == UI_INVALID_MOUNT)
        return 0;

    uint32_t removed = 0;
    for (auto it = bindings.begin(); it != bindings.end();)
    {
        if (it->second.desc.ownerMount == mountId)
        {
            it = bindings.erase(it);
            ++removed;
        }
        else
        {
            ++it;
        }
    }
    return removed;
}

void UiBindingManager::pruneInvalidNodes(const UiDocument& document)
{
    for (auto it = bindings.begin(); it != bindings.end();)
    {
        if (document.findNode(it->second.desc.target) == nullptr || !sourceAlive(it->second.desc.source))
            it = bindings.erase(it);
        else
            ++it;
    }
}

UiBindingFrameResult UiBindingManager::update(UiDocument& document,
                                              UiAnimationManager& animationManager)
{
    UiBindingFrameResult result;
    std::vector<UiBindingId> ids;
    ids.reserve(bindings.size());
    for (const auto& [id, _] : bindings)
        ids.push_back(id);
    std::sort(ids.begin(), ids.end());

    for (UiBindingId id : ids)
    {
        auto it = bindings.find(id);
        if (it == bindings.end())
            continue;

        UiBinding& binding = it->second;
        if (document.findNode(binding.desc.target) == nullptr || !sourceAlive(binding.desc.source))
        {
            bindings.erase(it);
            ++result.removed;
            continue;
        }

        ++result.evaluated;
        if (evaluateAndApply(binding, document, animationManager, false))
            ++result.applied;
    }

    result.active = static_cast<uint32_t>(bindings.size());
    return result;
}

const UiBinding* UiBindingManager::find(UiBindingId bindingId) const
{
    const auto it = bindings.find(bindingId);
    return it == bindings.end() ? nullptr : &it->second;
}

std::vector<UiBindingId> UiBindingManager::bindingsForTarget(UiHandle target) const
{
    std::vector<UiBindingId> result;
    for (const auto& [id, binding] : bindings)
    {
        if (binding.desc.target == target)
            result.push_back(id);
    }
    std::sort(result.begin(), result.end());
    return result;
}

UiBindingId UiBindingManager::nextId()
{
    return nextBindingId++;
}

bool UiBindingManager::sourceAlive(const UiBindingSource& source) const
{
    return source.alive == nullptr || source.alive();
}

bool UiBindingManager::shouldEvaluate(const UiBinding& binding, uint64_t revision) const
{
    return !binding.initialized ||
           binding.desc.source.revision == nullptr ||
           revision != binding.lastRevision;
}

bool UiBindingManager::evaluateAndApply(UiBinding& binding,
                                        UiDocument& document,
                                        UiAnimationManager& animationManager,
                                        bool initial)
{
    if (binding.desc.source.read == nullptr || !sourceAlive(binding.desc.source))
        return false;

    const uint64_t revision =
        binding.desc.source.revision == nullptr ? 0 : binding.desc.source.revision();
    if (!shouldEvaluate(binding, revision))
        return false;

    UiBindingValue value = binding.desc.source.read();
    if (binding.desc.transform)
        value = binding.desc.transform(value);

    if (binding.initialized && value == binding.lastValue)
    {
        binding.lastRevision = revision;
        return false;
    }

    const bool applied = applyValue(binding, document, animationManager, value, initial);
    if (applied)
    {
        binding.lastValue = value;
        binding.lastRevision = revision;
        binding.initialized = true;
    }
    return applied;
}

bool UiBindingManager::applyValue(UiBinding& binding,
                                  UiDocument& document,
                                  UiAnimationManager& animationManager,
                                  const UiBindingValue& value,
                                  bool initial) const
{
    switch (binding.desc.property)
    {
        case UiBindableProperty::Text:
            return applyText(document, binding.desc.target, value, binding.desc.formatter);

        case UiBindableProperty::Visible:
        {
            bool visible = false;
            return uiBindingValueAsBool(value, visible) && applyVisible(document, binding.desc.target, visible);
        }

        case UiBindableProperty::Enabled:
        {
            bool enabled = false;
            return uiBindingValueAsBool(value, enabled) && applyEnabled(document, binding.desc.target, enabled);
        }

        case UiBindableProperty::Interactable:
        {
            bool interactable = false;
            return uiBindingValueAsBool(value, interactable) &&
                   applyInteractable(document, binding.desc.target, interactable);
        }

        case UiBindableProperty::Opacity:
        {
            float opacity = 1.0f;
            return uiBindingValueAsFloat(value, opacity) &&
                   applyOpacity(binding, document, animationManager, opacity, initial);
        }

        case UiBindableProperty::Progress:
        {
            float progress = 0.0f;
            return uiBindingValueAsFloat(value, progress) &&
                   applyProgress(binding, document, animationManager, progress, initial);
        }

        case UiBindableProperty::RectColor:
        {
            UiColor color;
            return uiBindingValueAsColor(value, color) &&
                   applyRectColor(binding, document, animationManager, color, initial);
        }

        case UiBindableProperty::TextColor:
        {
            UiColor color;
            return uiBindingValueAsColor(value, color) &&
                   applyTextColor(binding, document, animationManager, color, initial);
        }

        case UiBindableProperty::ImageAsset:
        {
            std::string imageAsset;
            return uiBindingValueAsString(value, imageAsset) &&
                   applyImageAsset(document, binding.desc.target, imageAsset);
        }

        case UiBindableProperty::ImageTint:
        {
            UiColor tint;
            return uiBindingValueAsColor(value, tint) &&
                   applyImageTint(binding, document, animationManager, tint, initial);
        }

        case UiBindableProperty::StyleClass:
        {
            std::string styleClass;
            return uiBindingValueAsString(value, styleClass) &&
                   applyStyleClass(document, binding.desc.target, styleClass);
        }

        case UiBindableProperty::Layout:
        {
            const auto* layout = std::get_if<UiLayoutSpec>(&value);
            return layout != nullptr && applyLayout(document, binding.desc.target, *layout);
        }

        case UiBindableProperty::LayoutOffsetMin:
        {
            UiVec2 vec;
            return uiBindingValueAsVec2(value, vec) &&
                   applyLayoutVec2(binding,
                                   document,
                                   animationManager,
                                   UiAnimationProperty::LayoutOffsetMin,
                                   vec,
                                   initial);
        }

        case UiBindableProperty::LayoutOffsetMax:
        {
            UiVec2 vec;
            return uiBindingValueAsVec2(value, vec) &&
                   applyLayoutVec2(binding,
                                   document,
                                   animationManager,
                                   UiAnimationProperty::LayoutOffsetMax,
                                   vec,
                                   initial);
        }

        case UiBindableProperty::LayoutAnchorMin:
        {
            UiVec2 vec;
            return uiBindingValueAsVec2(value, vec) &&
                   applyLayoutVec2(binding,
                                   document,
                                   animationManager,
                                   UiAnimationProperty::LayoutAnchorMin,
                                   vec,
                                   initial);
        }

        case UiBindableProperty::LayoutAnchorMax:
        {
            UiVec2 vec;
            return uiBindingValueAsVec2(value, vec) &&
                   applyLayoutVec2(binding,
                                   document,
                                   animationManager,
                                   UiAnimationProperty::LayoutAnchorMax,
                                   vec,
                                   initial);
        }

        case UiBindableProperty::LayoutFixedSize:
        {
            UiVec2 vec;
            return uiBindingValueAsVec2(value, vec) &&
                   applyLayoutVec2(binding,
                                   document,
                                   animationManager,
                                   UiAnimationProperty::LayoutFixedSize,
                                   vec,
                                   initial);
        }

        case UiBindableProperty::LayoutContentOffset:
        {
            UiVec2 vec;
            return uiBindingValueAsVec2(value, vec) &&
                   applyLayoutVec2(binding,
                                   document,
                                   animationManager,
                                   UiAnimationProperty::LayoutContentOffset,
                                   vec,
                                   initial);
        }
    }

    return false;
}

bool UiBindingManager::applyText(UiDocument& document,
                                 UiHandle target,
                                 const UiBindingValue& value,
                                 const UiBindingFormatter& formatter) const
{
    UiNode* node = document.findNode(target);
    if (node == nullptr || node->type != UiNodeType::Text)
        return false;

    UiTextData data = std::holds_alternative<UiTextData>(node->payload)
        ? std::get<UiTextData>(node->payload)
        : UiTextData{};
    data.text = formatter ? formatter(value) : uiBindingValueToString(value);
    return document.setPayload(target, data);
}

bool UiBindingManager::applyVisible(UiDocument& document, UiHandle target, bool visible) const
{
    UiNode* node = document.findNode(target);
    if (node == nullptr)
        return false;

    UiStateFlags state = node->state;
    state.visible = visible;
    if (!visible)
    {
        state.hovered = false;
        state.focused = false;
        state.pressed = false;
    }
    return document.setState(target, state);
}

bool UiBindingManager::applyEnabled(UiDocument& document, UiHandle target, bool enabled) const
{
    UiNode* node = document.findNode(target);
    if (node == nullptr)
        return false;

    UiStateFlags state = node->state;
    state.enabled = enabled;
    if (!enabled)
    {
        state.hovered = false;
        state.focused = false;
        state.pressed = false;
    }
    return document.setState(target, state);
}

bool UiBindingManager::applyInteractable(UiDocument& document, UiHandle target, bool interactable) const
{
    UiNode* node = document.findNode(target);
    if (node == nullptr)
        return false;

    UiStateFlags state = node->state;
    state.interactable = interactable;
    return document.setState(target, state);
}

bool UiBindingManager::applyOpacity(UiBinding& binding,
                                    UiDocument& document,
                                    UiAnimationManager& animationManager,
                                    float opacity,
                                    bool initial) const
{
    opacity = clamp01(opacity);
    if (animate(binding, document, animationManager, UiAnimationProperty::Opacity, opacity, initial))
        return true;

    UiNode* node = document.findNode(binding.desc.target);
    if (node == nullptr)
        return false;

    UiStyle style = node->style;
    style.useOpacity = true;
    style.opacity = opacity;
    return document.setStyle(binding.desc.target, style);
}

bool UiBindingManager::applyProgress(UiBinding& binding,
                                     UiDocument& document,
                                     UiAnimationManager& animationManager,
                                     float progress,
                                     bool initial) const
{
    progress = clamp01(progress);
    UiNode* node = document.findNode(binding.desc.target);
    if (node == nullptr)
        return false;

    UiLayoutSpec layout = node->layout;
    layout.positionMode = UiPositionMode::Anchored;
    layout.widthMode = UiSizeMode::FromAnchors;
    layout.heightMode = UiSizeMode::FromAnchors;
    if (layout.anchorMax.y <= layout.anchorMin.y)
        layout.anchorMax.y = 1.0f;
    layout.anchorMax.x = progress;

    if (animate(binding,
                document,
                animationManager,
                UiAnimationProperty::LayoutAnchorMax,
                UiVec2{progress, layout.anchorMax.y},
                initial))
    {
        return true;
    }

    return document.setLayout(binding.desc.target, layout);
}

bool UiBindingManager::applyRectColor(UiBinding& binding,
                                      UiDocument& document,
                                      UiAnimationManager& animationManager,
                                      const UiColor& color,
                                      bool initial) const
{
    if (animate(binding, document, animationManager, UiAnimationProperty::RectColor, color, initial))
        return true;

    UiNode* node = document.findNode(binding.desc.target);
    auto* rect = node == nullptr ? nullptr : std::get_if<UiRectData>(&node->payload);
    if (rect == nullptr)
        return false;

    UiRectData data = *rect;
    data.color = color;
    return document.setPayload(binding.desc.target, data);
}

bool UiBindingManager::applyTextColor(UiBinding& binding,
                                      UiDocument& document,
                                      UiAnimationManager& animationManager,
                                      const UiColor& color,
                                      bool initial) const
{
    if (animate(binding, document, animationManager, UiAnimationProperty::TextColor, color, initial))
        return true;

    UiNode* node = document.findNode(binding.desc.target);
    auto* text = node == nullptr ? nullptr : std::get_if<UiTextData>(&node->payload);
    if (text == nullptr)
        return false;

    UiTextData data = *text;
    data.color = color;
    return document.setPayload(binding.desc.target, data);
}

bool UiBindingManager::applyImageAsset(UiDocument& document,
                                       UiHandle target,
                                       const std::string& imageAsset) const
{
    UiNode* node = document.findNode(target);
    auto* image = node == nullptr ? nullptr : std::get_if<UiImageData>(&node->payload);
    if (image == nullptr)
        return false;

    UiImageData data = *image;
    data.imageAsset = imageAsset;
    return document.setPayload(target, data);
}

bool UiBindingManager::applyImageTint(UiBinding& binding,
                                      UiDocument& document,
                                      UiAnimationManager& animationManager,
                                      const UiColor& tint,
                                      bool initial) const
{
    if (animate(binding, document, animationManager, UiAnimationProperty::ImageTint, tint, initial))
        return true;

    UiNode* node = document.findNode(binding.desc.target);
    auto* image = node == nullptr ? nullptr : std::get_if<UiImageData>(&node->payload);
    if (image == nullptr)
        return false;

    UiImageData data = *image;
    data.tint = tint;
    return document.setPayload(binding.desc.target, data);
}

bool UiBindingManager::applyStyleClass(UiDocument& document,
                                       UiHandle target,
                                       const std::string& styleClass) const
{
    UiNode* node = document.findNode(target);
    if (node == nullptr)
        return false;

    UiStyle style = node->style;
    style.styleClass = styleClass;
    return document.setStyle(target, style);
}

bool UiBindingManager::applyLayout(UiDocument& document, UiHandle target, const UiLayoutSpec& layout) const
{
    return document.setLayout(target, layout);
}

bool UiBindingManager::applyLayoutVec2(UiBinding& binding,
                                       UiDocument& document,
                                       UiAnimationManager& animationManager,
                                       UiAnimationProperty property,
                                       const UiVec2& value,
                                       bool initial) const
{
    if (animate(binding, document, animationManager, property, value, initial))
        return true;

    UiNode* node = document.findNode(binding.desc.target);
    if (node == nullptr)
        return false;

    UiLayoutSpec layout = node->layout;
    switch (property)
    {
        case UiAnimationProperty::LayoutOffsetMin:
            layout.offsetMin = value;
            break;
        case UiAnimationProperty::LayoutOffsetMax:
            layout.offsetMax = value;
            break;
        case UiAnimationProperty::LayoutAnchorMin:
            layout.anchorMin = value;
            layout.positionMode = UiPositionMode::Anchored;
            layout.widthMode = UiSizeMode::FromAnchors;
            layout.heightMode = UiSizeMode::FromAnchors;
            break;
        case UiAnimationProperty::LayoutAnchorMax:
            layout.anchorMax = value;
            layout.positionMode = UiPositionMode::Anchored;
            layout.widthMode = UiSizeMode::FromAnchors;
            layout.heightMode = UiSizeMode::FromAnchors;
            break;
        case UiAnimationProperty::LayoutFixedSize:
            layout.widthMode = UiSizeMode::Fixed;
            layout.heightMode = UiSizeMode::Fixed;
            layout.fixedWidth = std::max(0.0f, value.x);
            layout.fixedHeight = std::max(0.0f, value.y);
            break;
        case UiAnimationProperty::LayoutContentOffset:
            layout.contentOffset = value;
            break;
        default:
            return false;
    }
    return document.setLayout(binding.desc.target, layout);
}

bool UiBindingManager::animate(UiBinding& binding,
                               UiDocument& document,
                               UiAnimationManager& animationManager,
                               UiAnimationProperty property,
                               const UiAnimationValue& to,
                               bool initial) const
{
    if (!binding.desc.animation || (initial && !binding.desc.animateInitial))
        return false;

    UiAnimationDesc animation;
    animation.target = binding.desc.target;
    animation.property = property;
    animation.to = to;
    animation.timing = *binding.desc.animation;
    return animationManager.animate(document, std::move(animation)) != UI_INVALID_ANIMATION;
}
