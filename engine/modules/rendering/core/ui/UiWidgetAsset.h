#pragma once

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "UiSerialization.h"

inline constexpr int UI_WIDGET_ASSET_SCHEMA_VERSION = 1;

enum class UiWidgetAssetParameterType
{
    String,
    Number,
    Bool,
    Color,
    Asset,
    BindingPath
};

struct UiWidgetAssetParameter
{
    std::string name;
    UiWidgetAssetParameterType type = UiWidgetAssetParameterType::String;
    std::string defaultValue;
    std::string description;
    bool required = false;
};

struct UiWidgetAssetSlot
{
    std::string name;
    std::string target;
    std::string description;
    std::vector<UiSerializedWidget> defaultChildren;
};

struct UiWidgetAssetVariant
{
    std::string name;
    std::string displayName;
    std::string description;
    std::vector<std::string> tags;
    std::unordered_map<std::string, std::string> parameterDefaults;
    std::unordered_map<std::string, std::vector<UiSerializedWidget>> slotDefaults;
    UiSerializedWidget rootOverride;
    bool hasRootOverride = false;
};

struct UiWidgetAssetDefinition
{
    int schemaVersion = UI_WIDGET_ASSET_SCHEMA_VERSION;
    std::string id;
    int version = 1;
    std::string displayName;
    std::string description;
    std::vector<std::string> tags;
    std::vector<std::string> dependencies;
    std::string baseAsset;
    std::unordered_map<std::string, UiWidgetAssetParameter> parameters;
    std::unordered_map<std::string, UiWidgetAssetSlot> slots;
    std::unordered_map<std::string, UiWidgetAssetVariant> variants;
    UiSerializedWidget root;
};

struct UiWidgetAssetInstanceDesc
{
    std::string assetId;
    std::string variant;
    std::string idPrefix;
    std::unordered_map<std::string, std::string> parameterOverrides;
    std::unordered_map<std::string, std::vector<UiSerializedWidget>> slotContent;
};

struct UiWidgetAssetResolveResult
{
    bool success = false;
    UiSerializedValidationResult validation;
    UiWidgetAssetDefinition definition;
};

struct UiWidgetAssetExpandResult
{
    bool success = false;
    UiSerializedValidationResult validation;
    UiSerializedWidget widget;
};

bool parseUiWidgetAssetDefinition(const std::string& json,
                                  UiWidgetAssetDefinition& outAsset,
                                  UiSerializedValidationResult* outValidation = nullptr);
std::string serializeUiWidgetAssetDefinition(const UiWidgetAssetDefinition& asset);
bool loadUiWidgetAssetFromFile(const std::string& path,
                               UiWidgetAssetDefinition& outAsset,
                               UiSerializedValidationResult* outValidation = nullptr);
bool saveUiWidgetAssetToFile(const std::string& path,
                             const UiWidgetAssetDefinition& asset,
                             std::string* outError = nullptr);

class UiWidgetAssetRegistry
{
public:
    bool registerAsset(const UiWidgetAssetDefinition& asset,
                       UiSerializedValidationResult* outValidation = nullptr);
    bool unregisterAsset(const std::string& assetId);
    void clear();

    const UiWidgetAssetDefinition* findAsset(const std::string& assetId) const;
    std::vector<std::string> assetIds() const;

    UiSerializedValidationResult validateAsset(const UiWidgetAssetDefinition& asset) const;
    UiWidgetAssetResolveResult resolveAsset(const std::string& assetId,
                                            const std::string& variant = {}) const;
    UiWidgetAssetExpandResult expandAsset(const UiWidgetAssetInstanceDesc& desc) const;
    bool expandSerializedWidget(const UiSerializedWidget& widget,
                                UiSerializedWidget& outWidget,
                                UiSerializedValidationResult* outValidation = nullptr) const;
    bool expandSerializedAsset(const UiSerializedAsset& asset,
                               UiSerializedAsset& outAsset,
                               UiSerializedValidationResult* outValidation = nullptr) const;

    UiSerializedLoadResult instantiate(UiSystem& ui,
                                       UiSurfaceId surface,
                                       UiMountId mount,
                                       const UiWidgetAssetInstanceDesc& desc,
                                       const IUiSerializedBindingResolver* bindingResolver = nullptr,
                                       const UiTheme* validationTheme = nullptr) const;

private:
    UiWidgetAssetResolveResult resolveAssetRecursive(const std::string& assetId,
                                                     const std::string& variant,
                                                     std::vector<std::string>& stack) const;
    UiWidgetAssetExpandResult expandAssetRecursive(const UiWidgetAssetInstanceDesc& desc,
                                                   std::vector<std::string>& stack) const;
    bool expandSerializedWidgetRecursive(const UiSerializedWidget& widget,
                                         UiSerializedWidget& outWidget,
                                         UiSerializedValidationResult& validation,
                                         std::vector<std::string>& stack) const;

    std::unordered_map<std::string, UiWidgetAssetDefinition> assets;
};
