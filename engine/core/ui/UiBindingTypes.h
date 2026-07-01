#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>

#include "UiAnimationTypes.h"
#include "UiLayout.h"
#include "UiMountTypes.h"
#include "UiNode.h"
#include "UiTypes.h"

using UiBindingId = uint64_t;

static constexpr UiBindingId UI_INVALID_BINDING = 0;

enum class UiBindableProperty : uint8_t
{
    Text = 0,
    Visible,
    Enabled,
    Interactable,
    Opacity,
    Progress,
    RectColor,
    TextColor,
    ImageAsset,
    ImageTint,
    StyleClass,
    Layout,
    LayoutOffsetMin,
    LayoutOffsetMax,
    LayoutAnchorMin,
    LayoutAnchorMax,
    LayoutFixedSize,
    LayoutContentOffset
};

using UiBindingValue = std::variant<std::monostate,
                                    bool,
                                    int64_t,
                                    double,
                                    std::string,
                                    UiColor,
                                    UiVec2,
                                    UiLayoutSpec,
                                    UiNodePayload>;

struct UiBindingSource
{
    std::function<UiBindingValue()> read;
    std::function<uint64_t()> revision;
    std::function<bool()> alive;
};

using UiBindingTransform = std::function<UiBindingValue(const UiBindingValue&)>;
using UiBindingFormatter = std::function<std::string(const UiBindingValue&)>;

struct UiBindingDesc
{
    UiHandle target = UI_INVALID_HANDLE;
    UiBindableProperty property = UiBindableProperty::Text;
    UiBindingSource source;
    UiBindingTransform transform;
    UiBindingFormatter formatter;
    UiMountId ownerMount = UI_INVALID_MOUNT;
    UiHandle accessibilityTarget = UI_INVALID_HANDLE;
    std::string debugName;
    std::optional<UiAnimationTiming> animation;
    bool animateInitial = false;
    bool applyImmediately = true;
};

struct UiBindingFrameResult
{
    uint32_t active = 0;
    uint32_t evaluated = 0;
    uint32_t applied = 0;
    uint32_t removed = 0;

    bool changed() const
    {
        return applied > 0 || removed > 0;
    }
};

template <typename T>
class UiObservable
{
public:
    UiObservable() = default;

    explicit UiObservable(T initialValue)
        : currentValue(std::move(initialValue))
    {
    }

    const T& get() const
    {
        return currentValue;
    }

    void set(const T& value)
    {
        if (currentValue == value)
            return;

        currentValue = value;
        ++currentRevision;
    }

    void set(T&& value)
    {
        if (currentValue == value)
            return;

        currentValue = std::move(value);
        ++currentRevision;
    }

    uint64_t revision() const
    {
        return currentRevision;
    }

private:
    T currentValue{};
    uint64_t currentRevision = 1;
};

template <typename T>
UiBindingValue makeUiBindingValue(const T& value)
{
    using ValueType = std::decay_t<T>;

    if constexpr (std::is_same_v<ValueType, UiBindingValue>)
        return value;
    else if constexpr (std::is_same_v<ValueType, std::monostate>)
        return value;
    else if constexpr (std::is_same_v<ValueType, bool>)
        return value;
    else if constexpr (std::is_integral_v<ValueType>)
        return static_cast<int64_t>(value);
    else if constexpr (std::is_floating_point_v<ValueType>)
        return static_cast<double>(value);
    else if constexpr (std::is_same_v<ValueType, std::string>)
        return value;
    else if constexpr (std::is_same_v<ValueType, const char*>)
        return std::string(value == nullptr ? "" : value);
    else if constexpr (std::is_same_v<ValueType, UiColor>)
        return value;
    else if constexpr (std::is_same_v<ValueType, UiVec2>)
        return value;
    else if constexpr (std::is_same_v<ValueType, UiLayoutSpec>)
        return value;
    else if constexpr (std::is_same_v<ValueType, UiNodePayload>)
        return value;
    else
        static_assert(!sizeof(ValueType), "Unsupported UI binding value type");
}

template <typename T>
UiBindingSource bindObservable(UiObservable<T>& observable)
{
    UiBindingSource source;
    source.read = [&observable]()
    {
        return makeUiBindingValue(observable.get());
    };
    source.revision = [&observable]()
    {
        return observable.revision();
    };
    source.alive = []()
    {
        return true;
    };
    return source;
}

template <typename T>
UiBindingSource bindObservable(const std::shared_ptr<UiObservable<T>>& observable)
{
    std::weak_ptr<UiObservable<T>> weak = observable;
    UiBindingSource source;
    source.read = [weak]()
    {
        const std::shared_ptr<UiObservable<T>> locked = weak.lock();
        return locked == nullptr ? UiBindingValue{} : makeUiBindingValue(locked->get());
    };
    source.revision = [weak]()
    {
        const std::shared_ptr<UiObservable<T>> locked = weak.lock();
        return locked == nullptr ? 0u : locked->revision();
    };
    source.alive = [weak]()
    {
        return !weak.expired();
    };
    return source;
}

UiBindingSource makeBindingSource(std::function<UiBindingValue()> read,
                                  std::function<uint64_t()> revision = {},
                                  std::function<bool()> alive = {});

bool uiBindingValueAsBool(const UiBindingValue& value, bool& outValue);
bool uiBindingValueAsFloat(const UiBindingValue& value, float& outValue);
bool uiBindingValueAsString(const UiBindingValue& value, std::string& outValue);
bool uiBindingValueAsColor(const UiBindingValue& value, UiColor& outValue);
bool uiBindingValueAsVec2(const UiBindingValue& value, UiVec2& outValue);
std::string uiBindingValueToString(const UiBindingValue& value);
