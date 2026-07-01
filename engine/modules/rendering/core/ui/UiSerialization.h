#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <vector>

#include "UiAccessibilityTypes.h"
#include "UiAnimationTypes.h"
#include "UiBindingTypes.h"
#include "UiDragTypes.h"
#include "UiLayout.h"
#include "UiNavigationTypes.h"
#include "UiSurface.h"
#include "UiTheme.h"

class UiSystem;

inline constexpr int UI_SERIALIZATION_SCHEMA_VERSION = 1;

struct UiJsonValue
{
    using Object = std::vector<std::pair<std::string, UiJsonValue>>;
    using Array = std::vector<UiJsonValue>;

    std::variant<std::monostate, bool, double, std::string, Object, Array> value;

    bool isNull() const { return std::holds_alternative<std::monostate>(value); }
    bool isBool() const { return std::holds_alternative<bool>(value); }
    bool isNumber() const { return std::holds_alternative<double>(value); }
    bool isString() const { return std::holds_alternative<std::string>(value); }
    bool isObject() const { return std::holds_alternative<Object>(value); }
    bool isArray() const { return std::holds_alternative<Array>(value); }

    const UiJsonValue* find(const std::string& key) const;
};

bool parseUiJson(std::string_view source, UiJsonValue& outValue, std::string* outError = nullptr);
std::string serializeUiJson(const UiJsonValue& value, int indent = 0);

struct UiSerializedBinding
{
    UiBindableProperty property = UiBindableProperty::Text;
    std::string path;
    std::string formatter;
    std::string transform;
    std::optional<UiAnimationTiming> animation;
    bool animateInitial = false;
    bool applyImmediately = true;
};

struct UiSerializedNavigation
{
    bool enabled = false;
    bool focusable = true;
    UiNavigationRole role = UiNavigationRole::Generic;
    uint32_t scope = 0;
    std::string group;
    int tabIndex = UI_NAVIGATION_AUTO_TAB_INDEX;
    bool wrapNavigation = false;
    bool activateOnSubmit = true;
    std::unordered_map<UiNavigationDirection, std::string> neighbors;
};

struct UiSerializedDragSource
{
    bool enabled = true;
    std::string payloadType;
    uint64_t payloadId = 0;
    std::string payloadLabel;
    float startThreshold = 0.004f;
    bool capturePointer = true;
};

struct UiSerializedDropTarget
{
    bool enabled = true;
    bool acceptsAnyPayload = false;
    std::vector<std::string> acceptedPayloadTypes;
};

struct UiSerializedWidget
{
    std::string id;
    std::string type;
    UiLayoutSpec layout;
    std::string styleClass;
    std::string labelStyleClass;
    std::string text;
    std::string imageAsset;
    UiHorizontalAlign horizontalAlign = UiHorizontalAlign::Left;
    UiVerticalAlign verticalAlign = UiVerticalAlign::Top;
    UiTextWrapMode wrapMode = UiTextWrapMode::None;
    int maxLines = 0;
    UiColor imageTint = {1.0f, 1.0f, 1.0f, 1.0f};
    float imageAspect = 1.0f;
    float rotation = 0.0f;
    float progressValue = 0.0f;
    UiVec2 contentOffset = {0.0f, 0.0f};
    bool visible = true;
    bool enabled = true;
    bool interactable = false;
    bool decorative = true;
    UiSemanticDesc semantics;
    bool hasSemantics = false;
    UiSerializedNavigation navigation;
    std::optional<UiSerializedDragSource> dragSource;
    std::optional<UiSerializedDropTarget> dropTarget;
    std::optional<UiStyleTransitionDesc> stateTransition;
    std::vector<UiSerializedBinding> bindings;
    std::vector<UiSerializedWidget> children;
};

struct UiSerializedLayer
{
    std::string name;
    int order = 0;
    UiLayerState state;
};

struct UiSerializedSurface
{
    std::string name;
    UiSurfaceKind kind = UiSurfaceKind::Screen;
    int order = 0;
    UiRect rect = {0.0f, 0.0f, 1.0f, 1.0f};
    bool visible = true;
    bool enabled = true;
    bool inputEnabled = true;
    bool renderEnabled = true;
};

struct UiSerializedAsset
{
    int schemaVersion = UI_SERIALIZATION_SCHEMA_VERSION;
    std::string id;
    std::string theme;
    std::optional<UiSerializedSurface> surface;
    std::vector<UiSerializedLayer> layers;
    UiSerializedWidget root;
};

struct UiSerializedValidationIssue
{
    enum class Severity
    {
        Warning,
        Error
    };

    Severity severity = Severity::Error;
    std::string path;
    std::string message;
};

struct UiSerializedValidationResult
{
    std::vector<UiSerializedValidationIssue> issues;

    bool valid() const;
    void error(std::string path, std::string message);
    void warning(std::string path, std::string message);
};

struct UiSerializedInstance
{
    UiHandle root = UI_INVALID_HANDLE;
    std::unordered_map<std::string, UiHandle> handles;
};

struct UiSerializedLoadResult
{
    bool success = false;
    UiSerializedValidationResult validation;
    UiSerializedInstance instance;
};

class IUiSerializedBindingResolver
{
public:
    virtual ~IUiSerializedBindingResolver() = default;
    virtual std::optional<UiBindingSource> resolveBindingSource(const std::string& path) const = 0;
    virtual UiBindingFormatter resolveFormatter(const std::string& name) const
    {
        (void)name;
        return {};
    }
    virtual UiBindingTransform resolveTransform(const std::string& name) const
    {
        (void)name;
        return {};
    }
};

bool parseUiSerializedAsset(const std::string& json,
                            UiSerializedAsset& outAsset,
                            UiSerializedValidationResult* outValidation = nullptr);
std::string serializeUiSerializedAsset(const UiSerializedAsset& asset);
bool loadUiSerializedAssetFromFile(const std::string& path,
                                   UiSerializedAsset& outAsset,
                                   UiSerializedValidationResult* outValidation = nullptr);
bool saveUiSerializedAssetToFile(const std::string& path,
                                 const UiSerializedAsset& asset,
                                 std::string* outError = nullptr);

UiSerializedValidationResult validateUiSerializedAsset(const UiSerializedAsset& asset,
                                                       const UiTheme* theme = nullptr);

class UiSerializationRuntime
{
public:
    static UiSerializedLoadResult instantiate(UiSystem& ui,
                                              UiSurfaceId surface,
                                              UiMountId mount,
                                              const UiSerializedAsset& asset,
                                              const IUiSerializedBindingResolver* bindingResolver = nullptr,
                                              const UiTheme* validationTheme = nullptr);
};
