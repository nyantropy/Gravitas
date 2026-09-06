#pragma once

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "GtsJsonValue.h"

#include "UiAccessibilityTypes.h"
#include "UiAnimationTypes.h"
#include "UiBindingTypes.h"
#include "UiDragTypes.h"
#include "UiLayout.h"
#include "UiNavigationTypes.h"
#include "UiSurface.h"
#include "UiTheme.h"

class UiSystem;
class UiWidgetAssetRegistry;

inline constexpr int UI_SERIALIZATION_SCHEMA_VERSION = 1;

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

struct UiSerializedSemanticRelationships
{
    std::vector<std::string> labelledBy;
    std::vector<std::string> describedBy;
    std::vector<std::string> controls;
    std::vector<std::string> owns;
    std::string activeDescendant;
    std::string popup;
    std::string tooltip;
};

struct UiSerializedSemanticLocalizationRefs
{
    std::string nameKey;
    std::string descriptionKey;
    std::string hintKey;
    std::string valueKey;
    bool hasNameKey = false;
    bool hasDescriptionKey = false;
    bool hasHintKey = false;
    bool hasValueKey = false;

    bool any() const
    {
        return hasNameKey || hasDescriptionKey || hasHintKey || hasValueKey ||
               !nameKey.empty() || !descriptionKey.empty() || !hintKey.empty() || !valueKey.empty();
    }
};

struct UiSerializedWidget
{
    std::string id;
    std::string type;
    std::string asset;
    std::string variant;
    std::unordered_map<std::string, std::string> parameters;
    std::unordered_map<std::string, std::vector<UiSerializedWidget>> slots;
    UiLayoutSpec layout;
    bool hasLayout = false;
    std::string styleClass;
    std::string labelStyleClass;
    std::string text;
    std::string textKey;
    bool hasTextKey = false;
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
    bool hasVisible = false;
    bool hasEnabled = false;
    bool hasInteractable = false;
    bool hasDecorative = false;
    UiSemanticDesc semantics;
    bool hasSemantics = false;
    UiSerializedSemanticLocalizationRefs semanticLocalization;
    UiSerializedSemanticRelationships semanticRelationships;
    bool hasSemanticRelationships = false;
    UiSerializedNavigation navigation;
    bool hasNavigation = false;
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
bool parseUiSerializedWidget(const GtsJsonValue& json, UiSerializedWidget& outWidget);
GtsJsonValue serializeUiSerializedWidget(const UiSerializedWidget& widget);
bool loadUiSerializedAssetFromFile(const std::string& path,
                                   UiSerializedAsset& outAsset,
                                   UiSerializedValidationResult* outValidation = nullptr);
bool saveUiSerializedAssetToFile(const std::string& path,
                                 const UiSerializedAsset& asset,
                                 std::string* outError = nullptr);

UiSerializedValidationResult validateUiSerializedAsset(const UiSerializedAsset& asset,
                                                       const UiTheme* theme = nullptr);
UiSerializedValidationResult validateUiSerializedAsset(const UiSerializedAsset& asset,
                                                       const UiTheme* theme,
                                                       const UiWidgetAssetRegistry* widgetAssets);

class UiSerializationRuntime
{
public:
    static UiSerializedLoadResult instantiate(UiSystem& ui,
                                              UiSurfaceId surface,
                                              UiMountId mount,
                                              const UiSerializedAsset& asset,
                                              const IUiSerializedBindingResolver* bindingResolver = nullptr,
                                              const UiTheme* validationTheme = nullptr,
                                              const UiWidgetAssetRegistry* widgetAssets = nullptr);
};
