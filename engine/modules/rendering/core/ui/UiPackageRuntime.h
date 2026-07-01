#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "UiAssetRuntime.h"
#include "UiLocalizationRuntime.h"

class UiSystem;

inline constexpr int UI_PACKAGE_MANIFEST_SCHEMA_VERSION = 1;

struct UiPackageVersion
{
    int major = 0;
    int minor = 0;
    int patch = 0;
    std::string label;
    std::string text;
};

UiPackageVersion parseUiPackageVersion(const std::string& version);
int compareUiPackageVersions(const UiPackageVersion& lhs, const UiPackageVersion& rhs);

struct UiPackageDependency
{
    std::string packageId;
    std::string minVersion;
    bool optional = false;
};

struct UiPluginMetadata
{
    std::string id;
    std::string displayName;
    std::string version;
    std::string description;
    std::vector<std::string> capabilities;
    bool nativeCode = false;
};

struct UiPackageManifest
{
    int schemaVersion = UI_PACKAGE_MANIFEST_SCHEMA_VERSION;
    std::string id;
    std::string displayName;
    std::string author;
    std::string version = "1.0.0";
    std::string namespaceId;
    std::string engineCompatibility;
    std::string description;
    std::vector<std::string> tags;
    std::vector<std::string> assetRoots;
    std::vector<UiPackageDependency> dependencies;
    std::vector<UiPackageDependency> optionalDependencies;
    std::vector<UiPluginMetadata> plugins;
};

enum class UiPackageAssetOverridePolicy
{
    None,
    Replace
};

struct UiPackageAssetDesc
{
    UiAssetReference reference;
    UiPackageAssetOverridePolicy overridePolicy = UiPackageAssetOverridePolicy::None;
    std::filesystem::path sourcePath;
    std::vector<UiAssetReference> dependencies;

    std::optional<UiSerializedAsset> serializedAsset;
    std::optional<UiWidgetAssetDefinition> widgetAsset;
    std::optional<UiTheme> theme;
    std::optional<UiLocalizationAsset> localizationAsset;
};

struct UiPackageDesc
{
    UiPackageManifest manifest;
    std::vector<UiPackageAssetDesc> assets;
};

struct UiPackageAssetOrigin
{
    UiAssetReference reference;
    std::string packageId;
    UiPackageAssetOverridePolicy overridePolicy = UiPackageAssetOverridePolicy::None;
};

struct UiPackageRecord
{
    UiPackageDesc desc;
    UiSerializedValidationResult validation;
    uint32_t loadIndex = 0;
};

enum class UiPackageEventKind
{
    PackageLoaded,
    PackageUnloaded,
    PackageReloaded,
    PackageValidationFailed,
    DependencyResolved,
    OverrideApplied
};

struct UiPackageEvent
{
    UiPackageEventKind kind = UiPackageEventKind::PackageLoaded;
    std::string packageId;
    std::string dependencyId;
    UiAssetReference asset;
    std::string message;
    UiSerializedValidationResult validation;
};

struct UiPackageLoadResult
{
    bool success = true;
    UiSerializedValidationResult validation;
    std::vector<UiPackageEvent> events;
    UiAssetReloadResult assetReload;
};

bool parseUiPackageManifest(const std::string& json,
                            UiPackageManifest& outManifest,
                            UiSerializedValidationResult* outValidation = nullptr);
std::string serializeUiPackageManifest(const UiPackageManifest& manifest);

class UiPackageRuntime
{
public:
    UiPackageLoadResult registerPackage(UiSystem& ui, const UiPackageDesc& package);
    UiPackageLoadResult unregisterPackage(UiSystem& ui, const std::string& packageId);
    void clear();

    UiSerializedValidationResult validatePackage(const UiPackageDesc& package) const;
    const UiPackageRecord* findPackage(const std::string& packageId) const;
    std::vector<std::string> packageLoadOrder() const;

    UiAssetReference resolveAsset(UiAssetType type,
                                  const std::string& assetId,
                                  const std::string& requestingPackageId = {}) const;
    const UiPackageAssetOrigin* findAssetOrigin(const UiAssetReference& reference) const;
    std::vector<UiPackageAssetOrigin> assetOrigins() const;

    const std::vector<UiPackageEvent>& events() const { return lastEvents; }
    std::vector<UiPackageEvent> drainEvents();

private:
    struct EffectiveAsset
    {
        UiPackageAssetOrigin origin;
        UiPackageAssetDesc asset;
    };

    bool dependencySatisfied(const UiPackageDependency& dependency,
                             UiSerializedValidationResult& validation,
                             const std::string& path) const;
    bool packageDependsOn(const std::string& packageId,
                          const std::string& targetPackageId,
                          std::vector<std::string>& stack) const;
    UiSerializedValidationResult validatePackageAssets(const UiPackageDesc& package) const;
    std::unordered_map<std::string, EffectiveAsset> buildEffectiveAssetMap(std::vector<UiPackageEvent>& events) const;
    void applyEffectiveAssetChanges(UiSystem& ui,
                                    const std::unordered_map<std::string, EffectiveAsset>& nextEffective,
                                    const std::string& changedPackageId,
                                    UiPackageLoadResult& result);
    bool queuePackageAssetReload(UiSystem& ui, const UiPackageAssetDesc& asset);

    std::unordered_map<std::string, UiPackageRecord> packages;
    std::vector<std::string> loadOrder;
    std::unordered_map<std::string, EffectiveAsset> effectiveAssets;
    std::vector<UiPackageEvent> lastEvents;
    uint32_t nextLoadIndex = 1;
};
