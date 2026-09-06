#include "UiPackageRuntime.h"
#include "GtsJsonParser.h"

#include <algorithm>
#include <charconv>
#include <sstream>
#include <string_view>
#include <unordered_set>

#include "UiSystem.h"

namespace
{
    bool startsWith(const std::string& value, const std::string& prefix)
    {
        return value.size() >= prefix.size() && value.substr(0, prefix.size()) == prefix;
    }

    std::string packageAssetKey(const UiAssetReference& reference)
    {
        return uiAssetKey(reference);
    }

    std::vector<std::string> jsonStringArray(const GtsJsonValue* value)
    {
        std::vector<std::string> result;
        if (value == nullptr || !value->isArray())
            return result;

        for (const GtsJsonValue& item : value->asArray())
        {
            if (item.isString())
                result.push_back(item.asString());
        }
        return result;
    }

    UiPackageDependency parseDependency(const GtsJsonValue& value, bool optionalFallback)
    {
        UiPackageDependency dependency;
        dependency.optional = optionalFallback;
        if (value.isString())
        {
            dependency.packageId = value.asString();
            return dependency;
        }

        if (!value.isObject())
            return dependency;

        dependency.packageId = value.findString("id").value_or("");
        dependency.minVersion = value.findString("minVersion").value_or("");
        dependency.optional = value.findBool("optional").value_or(optionalFallback);
        return dependency;
    }

    std::vector<UiPackageDependency> parseDependencies(const GtsJsonValue* value, bool optional)
    {
        std::vector<UiPackageDependency> result;
        if (value == nullptr || !value->isArray())
            return result;

        for (const GtsJsonValue& item : value->asArray())
        {
            UiPackageDependency dependency = parseDependency(item, optional);
            if (!dependency.packageId.empty())
                result.push_back(std::move(dependency));
        }
        return result;
    }

    UiPluginMetadata parsePlugin(const GtsJsonValue& value)
    {
        UiPluginMetadata plugin;
        if (!value.isObject())
            return plugin;

        plugin.id = value.findString("id").value_or("");
        plugin.displayName = value.findString("displayName").value_or("");
        plugin.version = value.findString("version").value_or("");
        plugin.description = value.findString("description").value_or("");
        plugin.nativeCode = value.findBool("nativeCode").value_or(false);
        plugin.capabilities = jsonStringArray(value.find("capabilities"));
        return plugin;
    }

    std::vector<UiPluginMetadata> parsePlugins(const GtsJsonValue* value)
    {
        std::vector<UiPluginMetadata> result;
        if (value == nullptr || !value->isArray())
            return result;

        for (const GtsJsonValue& item : value->asArray())
        {
            UiPluginMetadata plugin = parsePlugin(item);
            if (!plugin.id.empty())
                result.push_back(std::move(plugin));
        }
        return result;
    }

    GtsJsonValue serializeStringArray(const std::vector<std::string>& values)
    {
        GtsJsonValue::Array array;
        for (const std::string& value : values)
            array.push_back(value);
        return array;
    }

    GtsJsonValue serializeDependency(const UiPackageDependency& dependency)
    {
        GtsJsonValue::Object object;
        object.emplace_back("id", dependency.packageId);
        if (!dependency.minVersion.empty())
            object.emplace_back("minVersion", dependency.minVersion);
        if (dependency.optional)
            object.emplace_back("optional", true);
        return object;
    }

    GtsJsonValue serializeDependencies(const std::vector<UiPackageDependency>& dependencies)
    {
        GtsJsonValue::Array array;
        for (const UiPackageDependency& dependency : dependencies)
            array.push_back(serializeDependency(dependency));
        return array;
    }

    GtsJsonValue serializePlugins(const std::vector<UiPluginMetadata>& plugins)
    {
        GtsJsonValue::Array array;
        for (const UiPluginMetadata& plugin : plugins)
        {
            GtsJsonValue::Object object;
            object.emplace_back("id", plugin.id);
            if (!plugin.displayName.empty())
                object.emplace_back("displayName", plugin.displayName);
            if (!plugin.version.empty())
                object.emplace_back("version", plugin.version);
            if (!plugin.description.empty())
                object.emplace_back("description", plugin.description);
            if (!plugin.capabilities.empty())
                object.emplace_back("capabilities", serializeStringArray(plugin.capabilities));
            if (plugin.nativeCode)
                object.emplace_back("nativeCode", true);
            array.push_back(std::move(object));
        }
        return array;
    }

    UiPackageEvent makePackageEvent(UiPackageEventKind kind,
                                    std::string packageId = {},
                                    std::string dependencyId = {},
                                    UiAssetReference asset = {},
                                    std::string message = {},
                                    UiSerializedValidationResult validation = {})
    {
        UiPackageEvent event;
        event.kind = kind;
        event.packageId = std::move(packageId);
        event.dependencyId = std::move(dependencyId);
        event.asset = std::move(asset);
        event.message = std::move(message);
        event.validation = std::move(validation);
        return event;
    }

    bool readIntComponent(std::string_view text, int& outValue)
    {
        if (text.empty())
            return false;
        const char* begin = text.data();
        const char* end = begin + text.size();
        int value = 0;
        const std::from_chars_result result = std::from_chars(begin, end, value);
        if (result.ec != std::errc() || result.ptr != end)
            return false;
        outValue = value;
        return true;
    }

    std::vector<std::string_view> splitVersion(std::string_view version)
    {
        std::vector<std::string_view> parts;
        size_t start = 0;
        while (start <= version.size())
        {
            const size_t dot = version.find('.', start);
            if (dot == std::string_view::npos)
            {
                parts.push_back(version.substr(start));
                break;
            }
            parts.push_back(version.substr(start, dot - start));
            start = dot + 1;
        }
        return parts;
    }

    bool containsPackage(const std::vector<std::string>& packages, const std::string& id)
    {
        return std::find(packages.begin(), packages.end(), id) != packages.end();
    }
}

UiPackageVersion parseUiPackageVersion(const std::string& version)
{
    UiPackageVersion parsed;
    parsed.text = version;
    const size_t labelPos = version.find_first_of("-+");
    const std::string_view numeric(version.data(), labelPos == std::string::npos ? version.size() : labelPos);
    if (labelPos != std::string::npos)
        parsed.label = version.substr(labelPos + 1);

    const std::vector<std::string_view> parts = splitVersion(numeric);
    if (!parts.empty())
        readIntComponent(parts[0], parsed.major);
    if (parts.size() > 1)
        readIntComponent(parts[1], parsed.minor);
    if (parts.size() > 2)
        readIntComponent(parts[2], parsed.patch);
    return parsed;
}

int compareUiPackageVersions(const UiPackageVersion& lhs, const UiPackageVersion& rhs)
{
    if (lhs.major != rhs.major)
        return lhs.major < rhs.major ? -1 : 1;
    if (lhs.minor != rhs.minor)
        return lhs.minor < rhs.minor ? -1 : 1;
    if (lhs.patch != rhs.patch)
        return lhs.patch < rhs.patch ? -1 : 1;
    return 0;
}

bool parseUiPackageManifest(const std::string& json,
                            UiPackageManifest& outManifest,
                            UiSerializedValidationResult* outValidation)
{
    UiSerializedValidationResult validation;
    GtsJsonValue root;
    std::string parseError;
    if (!GtsJsonParser::parse(json, root, &parseError))
    {
        validation.error("$", parseError.empty() ? "package manifest JSON parse failed" : parseError);
        if (outValidation != nullptr)
            *outValidation = validation;
        return false;
    }

    if (!root.isObject())
    {
        validation.error("$", "package manifest root must be an object");
        if (outValidation != nullptr)
            *outValidation = validation;
        return false;
    }

    UiPackageManifest manifest;
    manifest.schemaVersion = root.findInt32("schema").value_or(UI_PACKAGE_MANIFEST_SCHEMA_VERSION);
    manifest.id = root.findString("id").value_or("");
    manifest.displayName = root.findString("displayName").value_or("");
    manifest.author = root.findString("author").value_or("");
    manifest.version = root.findString("version").value_or("1.0.0");
    manifest.namespaceId = root.findString("namespace").value_or("");
    manifest.engineCompatibility = root.findString("engineCompatibility").value_or("");
    manifest.description = root.findString("description").value_or("");
    manifest.tags = jsonStringArray(root.find("tags"));
    manifest.assetRoots = jsonStringArray(root.find("assetRoots"));
    manifest.dependencies = parseDependencies(root.find("dependencies"), false);
    manifest.optionalDependencies = parseDependencies(root.find("optionalDependencies"), true);
    manifest.plugins = parsePlugins(root.find("plugins"));

    if (manifest.id.empty())
        validation.error("$.id", "package id is required");
    if (manifest.namespaceId.empty())
        validation.error("$.namespace", "package namespace is required");
    if (manifest.schemaVersion != UI_PACKAGE_MANIFEST_SCHEMA_VERSION)
        validation.error("$.schema", "package manifest schema version is not supported");

    outManifest = std::move(manifest);
    if (outValidation != nullptr)
        *outValidation = validation;
    return validation.valid();
}

std::string serializeUiPackageManifest(const UiPackageManifest& manifest)
{
    GtsJsonValue::Object root;
    root.emplace_back("schema", manifest.schemaVersion);
    root.emplace_back("id", manifest.id);
    root.emplace_back("version", manifest.version);
    root.emplace_back("namespace", manifest.namespaceId);
    if (!manifest.displayName.empty())
        root.emplace_back("displayName", manifest.displayName);
    if (!manifest.author.empty())
        root.emplace_back("author", manifest.author);
    if (!manifest.engineCompatibility.empty())
        root.emplace_back("engineCompatibility", manifest.engineCompatibility);
    if (!manifest.description.empty())
        root.emplace_back("description", manifest.description);
    if (!manifest.tags.empty())
        root.emplace_back("tags", serializeStringArray(manifest.tags));
    if (!manifest.assetRoots.empty())
        root.emplace_back("assetRoots", serializeStringArray(manifest.assetRoots));
    if (!manifest.dependencies.empty())
        root.emplace_back("dependencies", serializeDependencies(manifest.dependencies));
    if (!manifest.optionalDependencies.empty())
        root.emplace_back("optionalDependencies", serializeDependencies(manifest.optionalDependencies));
    if (!manifest.plugins.empty())
        root.emplace_back("plugins", serializePlugins(manifest.plugins));
    return GtsJsonParser::serialize(std::move(root));
}

UiPackageLoadResult UiPackageRuntime::registerPackage(UiSystem& ui, const UiPackageDesc& package)
{
    UiPackageLoadResult result;
    result.validation = validatePackage(package);
    if (!result.validation.valid())
    {
        result.success = false;
        result.events.push_back(makePackageEvent(UiPackageEventKind::PackageValidationFailed,
                                                 package.manifest.id,
                                                 {},
                                                 {},
                                                 "package validation failed",
                                                 result.validation));
        lastEvents = result.events;
        return result;
    }

    const bool existed = packages.find(package.manifest.id) != packages.end();
    UiPackageRecord record;
    record.desc = package;
    record.validation = result.validation;
    record.loadIndex = existed ? packages.at(package.manifest.id).loadIndex : nextLoadIndex++;
    packages[package.manifest.id] = std::move(record);
    if (!containsPackage(loadOrder, package.manifest.id))
        loadOrder.push_back(package.manifest.id);

    for (const UiPackageDependency& dependency : package.manifest.dependencies)
    {
        result.events.push_back(makePackageEvent(UiPackageEventKind::DependencyResolved,
                                                 package.manifest.id,
                                                 dependency.packageId));
    }
    for (const UiPackageDependency& dependency : package.manifest.optionalDependencies)
    {
        if (packages.find(dependency.packageId) != packages.end())
        {
            result.events.push_back(makePackageEvent(UiPackageEventKind::DependencyResolved,
                                                     package.manifest.id,
                                                     dependency.packageId));
        }
    }

    const std::unordered_map<std::string, EffectiveAsset> nextEffective =
        buildEffectiveAssetMap(result.events);
    applyEffectiveAssetChanges(ui, nextEffective, package.manifest.id, result);
    result.events.push_back(makePackageEvent(existed ? UiPackageEventKind::PackageReloaded
                                                     : UiPackageEventKind::PackageLoaded,
                                             package.manifest.id));
    result.success = result.success && result.validation.valid() && result.assetReload.success;
    lastEvents = result.events;
    return result;
}

UiPackageLoadResult UiPackageRuntime::unregisterPackage(UiSystem& ui, const std::string& packageId)
{
    UiPackageLoadResult result;
    const auto it = packages.find(packageId);
    if (it == packages.end())
    {
        result.success = false;
        result.validation.error("$.package", "package '" + packageId + "' is not registered");
        result.events.push_back(makePackageEvent(UiPackageEventKind::PackageValidationFailed,
                                                 packageId,
                                                 {},
                                                 {},
                                                 "package is not registered",
                                                 result.validation));
        lastEvents = result.events;
        return result;
    }

    packages.erase(it);
    loadOrder.erase(std::remove(loadOrder.begin(), loadOrder.end(), packageId), loadOrder.end());
    const std::unordered_map<std::string, EffectiveAsset> nextEffective =
        buildEffectiveAssetMap(result.events);
    applyEffectiveAssetChanges(ui, nextEffective, packageId, result);
    result.events.push_back(makePackageEvent(UiPackageEventKind::PackageUnloaded, packageId));
    result.success = result.assetReload.success;
    lastEvents = result.events;
    return result;
}

void UiPackageRuntime::clear()
{
    packages.clear();
    loadOrder.clear();
    effectiveAssets.clear();
    lastEvents.clear();
    nextLoadIndex = 1;
}

UiSerializedValidationResult UiPackageRuntime::validatePackage(const UiPackageDesc& package) const
{
    UiSerializedValidationResult validation;
    const UiPackageManifest& manifest = package.manifest;
    if (manifest.schemaVersion != UI_PACKAGE_MANIFEST_SCHEMA_VERSION)
        validation.error("$.schema", "package manifest schema version is not supported");
    if (manifest.id.empty())
        validation.error("$.id", "package id is required");
    if (manifest.version.empty())
        validation.error("$.version", "package version is required");
    if (manifest.namespaceId.empty())
        validation.error("$.namespace", "package namespace is required");

    for (size_t i = 0; i < manifest.dependencies.size(); ++i)
        dependencySatisfied(manifest.dependencies[i], validation, "$.dependencies[" + std::to_string(i) + "]");
    for (size_t i = 0; i < manifest.optionalDependencies.size(); ++i)
    {
        const UiPackageDependency& dependency = manifest.optionalDependencies[i];
        if (packages.find(dependency.packageId) != packages.end())
            dependencySatisfied(dependency, validation, "$.optionalDependencies[" + std::to_string(i) + "]");
    }

    for (const UiPackageDependency& dependency : manifest.dependencies)
    {
        std::vector<std::string> stack;
        if (dependency.packageId == manifest.id || packageDependsOn(dependency.packageId, manifest.id, stack))
            validation.error("$.dependencies", "package dependency cycle involving '" + dependency.packageId + "'");
    }

    UiSerializedValidationResult assetValidation = validatePackageAssets(package);
    validation.issues.insert(validation.issues.end(),
                             assetValidation.issues.begin(),
                             assetValidation.issues.end());
    return validation;
}

const UiPackageRecord* UiPackageRuntime::findPackage(const std::string& packageId) const
{
    const auto it = packages.find(packageId);
    return it == packages.end() ? nullptr : &it->second;
}

std::vector<std::string> UiPackageRuntime::packageLoadOrder() const
{
    return loadOrder;
}

UiAssetReference UiPackageRuntime::resolveAsset(UiAssetType type,
                                                const std::string& assetId,
                                                const std::string& requestingPackageId) const
{
    if (assetId.empty())
        return {};

    UiAssetReference exact{type, assetId};
    if (findAssetOrigin(exact) != nullptr)
        return exact;

    const UiPackageRecord* package = findPackage(requestingPackageId);
    if (package != nullptr && !package->desc.manifest.namespaceId.empty())
    {
        UiAssetReference namespaced{type, package->desc.manifest.namespaceId + "." + assetId};
        if (findAssetOrigin(namespaced) != nullptr)
            return namespaced;
    }

    return exact;
}

const UiPackageAssetOrigin* UiPackageRuntime::findAssetOrigin(const UiAssetReference& reference) const
{
    const auto it = effectiveAssets.find(packageAssetKey(reference));
    return it == effectiveAssets.end() ? nullptr : &it->second.origin;
}

std::vector<UiPackageAssetOrigin> UiPackageRuntime::assetOrigins() const
{
    std::vector<UiPackageAssetOrigin> result;
    result.reserve(effectiveAssets.size());
    for (const auto& [_, effective] : effectiveAssets)
        result.push_back(effective.origin);
    std::sort(result.begin(), result.end(), [](const UiPackageAssetOrigin& lhs, const UiPackageAssetOrigin& rhs)
    {
        return packageAssetKey(lhs.reference) < packageAssetKey(rhs.reference);
    });
    return result;
}

std::vector<UiPackageEvent> UiPackageRuntime::drainEvents()
{
    std::vector<UiPackageEvent> drained = std::move(lastEvents);
    lastEvents.clear();
    return drained;
}

bool UiPackageRuntime::dependencySatisfied(const UiPackageDependency& dependency,
                                           UiSerializedValidationResult& validation,
                                           const std::string& path) const
{
    const auto it = packages.find(dependency.packageId);
    if (it == packages.end())
    {
        if (!dependency.optional)
            validation.error(path, "required package '" + dependency.packageId + "' is not loaded");
        return dependency.optional;
    }

    if (!dependency.minVersion.empty())
    {
        const UiPackageVersion loaded = parseUiPackageVersion(it->second.desc.manifest.version);
        const UiPackageVersion required = parseUiPackageVersion(dependency.minVersion);
        if (compareUiPackageVersions(loaded, required) < 0)
        {
            validation.error(path,
                             "package '" + dependency.packageId + "' version " +
                                 it->second.desc.manifest.version +
                                 " does not satisfy minimum " + dependency.minVersion);
            return false;
        }
    }
    return true;
}

bool UiPackageRuntime::packageDependsOn(const std::string& packageId,
                                        const std::string& targetPackageId,
                                        std::vector<std::string>& stack) const
{
    if (std::find(stack.begin(), stack.end(), packageId) != stack.end())
        return false;
    stack.push_back(packageId);

    const auto it = packages.find(packageId);
    if (it == packages.end())
        return false;

    const UiPackageManifest& manifest = it->second.desc.manifest;
    for (const UiPackageDependency& dependency : manifest.dependencies)
    {
        if (dependency.packageId == targetPackageId ||
            packageDependsOn(dependency.packageId, targetPackageId, stack))
            return true;
    }
    for (const UiPackageDependency& dependency : manifest.optionalDependencies)
    {
        if (packages.find(dependency.packageId) != packages.end() &&
            (dependency.packageId == targetPackageId ||
             packageDependsOn(dependency.packageId, targetPackageId, stack)))
            return true;
    }
    return false;
}

UiSerializedValidationResult UiPackageRuntime::validatePackageAssets(const UiPackageDesc& package) const
{
    UiSerializedValidationResult validation;
    std::unordered_set<std::string> packageAssetIds;
    const std::string namespacePrefix = package.manifest.namespaceId.empty()
        ? std::string{}
        : package.manifest.namespaceId + ".";

    for (size_t i = 0; i < package.assets.size(); ++i)
    {
        const UiPackageAssetDesc& asset = package.assets[i];
        const std::string path = "$.assets[" + std::to_string(i) + "]";
        if (!asset.reference.valid())
        {
            validation.error(path, "package asset reference is required");
            continue;
        }

        if (!namespacePrefix.empty() && !startsWith(asset.reference.id, namespacePrefix))
        {
            validation.warning(path + ".id",
                               "asset id '" + asset.reference.id + "' is outside package namespace '" +
                                   package.manifest.namespaceId + "'");
        }

        const std::string key = packageAssetKey(asset.reference);
        if (!packageAssetIds.insert(key).second)
            validation.error(path, "package contains duplicate asset '" + key + "'");

        const auto effectiveIt = effectiveAssets.find(key);
        if (effectiveIt != effectiveAssets.end() &&
            effectiveIt->second.origin.packageId != package.manifest.id &&
            asset.overridePolicy != UiPackageAssetOverridePolicy::Replace)
        {
            validation.error(path, "asset '" + key + "' already exists and no override policy was provided");
        }

        if (asset.reference.type == UiAssetType::SerializedUi && !asset.serializedAsset)
            validation.error(path, "serialized UI package asset is missing serialized data");
        if (asset.reference.type == UiAssetType::WidgetAsset && !asset.widgetAsset)
            validation.error(path, "widget package asset is missing widget asset data");
        if (asset.reference.type == UiAssetType::Theme && !asset.theme)
            validation.error(path, "theme package asset is missing theme data");
        if (asset.reference.type == UiAssetType::Localization)
        {
            if (!asset.localizationAsset)
            {
                validation.error(path, "localization package asset is missing localization data");
            }
            else
            {
                UiSerializedValidationResult catalogValidation =
                    UiLocalizationRuntime{}.validateCatalog(*asset.localizationAsset);
                validation.issues.insert(validation.issues.end(),
                                         catalogValidation.issues.begin(),
                                         catalogValidation.issues.end());
                if (asset.localizationAsset->asset != asset.reference)
                    validation.error(path, "localization package asset reference does not match catalog id");
                if (asset.localizationAsset->packageId != package.manifest.id)
                    validation.error(path, "localization catalog package does not match package manifest id");
                if (asset.localizationAsset->namespaceId != package.manifest.namespaceId)
                    validation.error(path, "localization catalog namespace does not match package manifest namespace");
            }
        }
        if (asset.reference.type == UiAssetType::Unknown)
            validation.error(path, "package asset type is unknown");
    }
    return validation;
}

std::unordered_map<std::string, UiPackageRuntime::EffectiveAsset>
UiPackageRuntime::buildEffectiveAssetMap(std::vector<UiPackageEvent>& events) const
{
    std::unordered_map<std::string, EffectiveAsset> result;
    for (const std::string& packageId : loadOrder)
    {
        const auto packageIt = packages.find(packageId);
        if (packageIt == packages.end())
            continue;

        for (const UiPackageAssetDesc& asset : packageIt->second.desc.assets)
        {
            const std::string key = packageAssetKey(asset.reference);
            const auto existing = result.find(key);
            if (existing != result.end() && asset.overridePolicy == UiPackageAssetOverridePolicy::Replace)
            {
                events.push_back(makePackageEvent(UiPackageEventKind::OverrideApplied,
                                                  packageId,
                                                  existing->second.origin.packageId,
                                                  asset.reference));
            }
            else if (existing != result.end())
            {
                continue;
            }

            EffectiveAsset effective;
            effective.origin.reference = asset.reference;
            effective.origin.packageId = packageId;
            effective.origin.overridePolicy = asset.overridePolicy;
            effective.asset = asset;
            result[key] = std::move(effective);
        }
    }
    return result;
}

void UiPackageRuntime::applyEffectiveAssetChanges(
    UiSystem& ui,
    const std::unordered_map<std::string, EffectiveAsset>& nextEffective,
    const std::string& changedPackageId,
    UiPackageLoadResult& result)
{
    for (const auto& [key, current] : effectiveAssets)
    {
        if (nextEffective.find(key) == nextEffective.end())
        {
            if (current.origin.reference.type == UiAssetType::Localization)
                ui.localization().unregisterCatalog(current.origin.reference);
            else
                ui.uiAssets().unregisterAsset(ui, current.origin.reference, &result.assetReload);
        }
    }

    for (const auto& [key, next] : nextEffective)
    {
        const auto currentIt = effectiveAssets.find(key);
        const bool changedProvider =
            currentIt == effectiveAssets.end() ||
            currentIt->second.origin.packageId != next.origin.packageId ||
            next.origin.packageId == changedPackageId;
        if (changedProvider)
        {
            if (next.asset.reference.type == UiAssetType::Localization && next.asset.localizationAsset)
            {
                UiSerializedValidationResult validation;
                const bool registered = ui.localization().registerCatalog(*next.asset.localizationAsset,
                                                                          &validation);
                if (!registered || !validation.valid())
                {
                    result.success = false;
                    result.validation.issues.insert(result.validation.issues.end(),
                                                    validation.issues.begin(),
                                                    validation.issues.end());
                    result.events.push_back(makePackageEvent(UiPackageEventKind::PackageValidationFailed,
                                                             next.origin.packageId,
                                                             {},
                                                             next.asset.reference,
                                                             "localization catalog failed validation",
                                                             validation));
                }
            }
            else
            {
                queuePackageAssetReload(ui, next.asset);
            }
        }
    }

    UiAssetReloadResult reload = ui.processUiAssetReloads();
    result.assetReload.success = result.assetReload.success && reload.success;
    result.assetReload.events.insert(result.assetReload.events.end(), reload.events.begin(), reload.events.end());
    result.assetReload.invalidatedAssets.insert(result.assetReload.invalidatedAssets.end(),
                                                reload.invalidatedAssets.begin(),
                                                reload.invalidatedAssets.end());
    result.assetReload.rebuiltConsumers += reload.rebuiltConsumers;
    result.assetReload.failedConsumers += reload.failedConsumers;
    result.success = result.success && reload.success;
    effectiveAssets = nextEffective;
}

bool UiPackageRuntime::queuePackageAssetReload(UiSystem& ui, const UiPackageAssetDesc& asset)
{
    if (asset.reference.type == UiAssetType::SerializedUi && asset.serializedAsset)
        return ui.uiAssets().queueSerializedAssetReload(*asset.serializedAsset, asset.sourcePath);
    if (asset.reference.type == UiAssetType::WidgetAsset && asset.widgetAsset)
        return ui.uiAssets().queueWidgetAssetReload(*asset.widgetAsset, asset.sourcePath);
    if (asset.reference.type == UiAssetType::Theme && asset.theme)
        return ui.uiAssets().queueThemeReload(asset.reference.id, *asset.theme, asset.dependencies, asset.sourcePath);
    if (asset.reference.type == UiAssetType::Localization && asset.localizationAsset)
    {
        UiSerializedValidationResult validation;
        return ui.localization().registerCatalog(*asset.localizationAsset, &validation);
    }
    return false;
}
