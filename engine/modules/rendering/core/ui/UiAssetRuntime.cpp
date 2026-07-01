#include "UiAssetRuntime.h"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <string_view>

#include "UiSystem.h"

namespace
{
    bool startsWith(const std::string& value, const char* prefix)
    {
        const std::string_view text(value);
        const std::string_view start(prefix);
        return text.size() >= start.size() && text.substr(0, start.size()) == start;
    }

    void appendUnique(std::vector<UiAssetReference>& refs, const UiAssetReference& ref)
    {
        if (!ref.valid())
            return;
        if (std::find(refs.begin(), refs.end(), ref) == refs.end())
            refs.push_back(ref);
    }

    void appendUnique(std::vector<UiAssetReference>& refs, const std::vector<UiAssetReference>& more)
    {
        for (const UiAssetReference& ref : more)
            appendUnique(refs, ref);
    }

    void collectWidgetReferences(const UiSerializedWidget& widget, std::vector<UiAssetReference>& refs)
    {
        if (!widget.asset.empty())
            appendUnique(refs, UiAssetReference{UiAssetType::WidgetAsset, widget.asset});

        for (const UiSerializedWidget& child : widget.children)
            collectWidgetReferences(child, refs);

        for (const auto& [_, children] : widget.slots)
        {
            for (const UiSerializedWidget& child : children)
                collectWidgetReferences(child, refs);
        }
    }

    std::vector<UiAssetReference> collectSerializedAssetDependencies(const UiSerializedAsset& asset)
    {
        std::vector<UiAssetReference> refs;
        if (!asset.theme.empty())
            appendUnique(refs, UiAssetReference{UiAssetType::Theme, asset.theme});
        collectWidgetReferences(asset.root, refs);
        return refs;
    }

    std::vector<UiAssetReference> collectWidgetAssetDependencies(const UiWidgetAssetDefinition& asset)
    {
        std::vector<UiAssetReference> refs;
        if (!asset.baseAsset.empty())
            appendUnique(refs, UiAssetReference{UiAssetType::WidgetAsset, asset.baseAsset});
        for (const std::string& dependency : asset.dependencies)
            appendUnique(refs, uiAssetReferenceFromDependencyString(dependency));
        collectWidgetReferences(asset.root, refs);
        return refs;
    }

    std::string readTextFile(const std::filesystem::path& path)
    {
        std::ifstream file(path);
        if (!file)
            return {};

        std::ostringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }

    bool containsReference(const std::vector<UiAssetReference>& refs, const UiAssetReference& ref)
    {
        return std::find(refs.begin(), refs.end(), ref) != refs.end();
    }

    UiAssetReloadEvent makeReloadEvent(UiAssetReloadEventKind kind,
                                       UiAssetReference asset = {},
                                       std::string consumerId = {},
                                       std::string message = {},
                                       UiSerializedValidationResult validation = {},
                                       uint64_t runtimeVersion = 0)
    {
        UiAssetReloadEvent event;
        event.kind = kind;
        event.asset = std::move(asset);
        event.consumerId = std::move(consumerId);
        event.message = std::move(message);
        event.validation = std::move(validation);
        event.runtimeVersion = runtimeVersion;
        return event;
    }
}

std::string uiAssetTypeName(UiAssetType type)
{
    switch (type)
    {
    case UiAssetType::SerializedUi:
        return "SerializedUi";
    case UiAssetType::WidgetAsset:
        return "WidgetAsset";
    case UiAssetType::Theme:
        return "Theme";
    default:
        return "Unknown";
    }
}

std::string uiAssetKey(const UiAssetReference& reference)
{
    return uiAssetTypeName(reference.type) + ":" + reference.id;
}

UiAssetReference uiAssetReferenceFromDependencyString(const std::string& dependency)
{
    if (startsWith(dependency, "theme:"))
        return UiAssetReference{UiAssetType::Theme, dependency.substr(6)};
    if (startsWith(dependency, "widget:"))
        return UiAssetReference{UiAssetType::WidgetAsset, dependency.substr(7)};
    if (startsWith(dependency, "ui:"))
        return UiAssetReference{UiAssetType::SerializedUi, dependency.substr(3)};
    return UiAssetReference{UiAssetType::WidgetAsset, dependency};
}

std::string uiWidgetAssetConsumerId(const UiWidgetAssetInstanceDesc& desc,
                                    UiSurfaceId surface,
                                    UiMountId mount)
{
    return "widget:" + desc.assetId + ":surface=" + std::to_string(surface) +
           ":mount=" + std::to_string(mount);
}

std::string uiSerializedAssetConsumerId(const UiSerializedAsset& asset,
                                        UiSurfaceId surface,
                                        UiMountId mount)
{
    const std::string assetId = asset.id.empty() ? "anonymous" : asset.id;
    return "serialized:" + assetId + ":surface=" + std::to_string(surface) +
           ":mount=" + std::to_string(mount);
}

bool UiAssetRuntime::registerSerializedAsset(UiSystem& ui,
                                             const UiSerializedAsset& asset,
                                             UiSerializedValidationResult* outValidation,
                                             std::filesystem::path sourcePath)
{
    std::vector<UiAssetReference> changed;
    return applySerializedAsset(ui, asset, sourcePath, outValidation, nullptr, &changed);
}

bool UiAssetRuntime::registerWidgetAsset(UiSystem& ui,
                                         const UiWidgetAssetDefinition& asset,
                                         UiSerializedValidationResult* outValidation,
                                         std::filesystem::path sourcePath)
{
    std::vector<UiAssetReference> changed;
    return applyWidgetAsset(ui, asset, sourcePath, outValidation, nullptr, &changed);
}

bool UiAssetRuntime::registerThemeAsset(UiSystem& ui,
                                        const std::string& id,
                                        const UiTheme& theme,
                                        std::vector<UiAssetReference> dependencies,
                                        std::filesystem::path sourcePath)
{
    std::vector<UiAssetReference> changed;
    return applyThemeAsset(ui, id, theme, dependencies, sourcePath, nullptr, &changed);
}

bool UiAssetRuntime::queueSerializedAssetReload(const UiSerializedAsset& asset, std::filesystem::path sourcePath)
{
    if (asset.id.empty())
        return false;
    PendingReload reload;
    reload.type = UiAssetType::SerializedUi;
    reload.serializedAsset = asset;
    reload.sourcePath = std::move(sourcePath);
    pendingReloads.push_back(std::move(reload));
    return true;
}

bool UiAssetRuntime::queueWidgetAssetReload(const UiWidgetAssetDefinition& asset, std::filesystem::path sourcePath)
{
    if (asset.id.empty())
        return false;
    PendingReload reload;
    reload.type = UiAssetType::WidgetAsset;
    reload.widgetAsset = asset;
    reload.sourcePath = std::move(sourcePath);
    pendingReloads.push_back(std::move(reload));
    return true;
}

bool UiAssetRuntime::queueThemeReload(const std::string& id,
                                      const UiTheme& theme,
                                      std::vector<UiAssetReference> dependencies,
                                      std::filesystem::path sourcePath)
{
    if (id.empty())
        return false;
    PendingReload reload;
    reload.type = UiAssetType::Theme;
    reload.themeId = id;
    reload.theme = theme;
    reload.dependencies = std::move(dependencies);
    reload.sourcePath = std::move(sourcePath);
    pendingReloads.push_back(std::move(reload));
    return true;
}

bool UiAssetRuntime::queueReloadFromSource(const UiAssetReference& reference)
{
    if (!reference.valid())
        return false;
    PendingReload reload;
    reload.type = reference.type;
    reload.sourceReference = reference;
    reload.fromSource = true;
    pendingReloads.push_back(std::move(reload));
    return true;
}

UiAssetReloadResult UiAssetRuntime::processReloads(UiSystem& ui)
{
    UiAssetReloadResult result;
    lastEvents.clear();

    std::vector<PendingReload> queue = std::move(pendingReloads);
    pendingReloads.clear();

    std::vector<UiAssetReference> changed;
    for (const PendingReload& reload : queue)
    {
        bool applied = false;
        if (reload.fromSource)
        {
            applied = applyReloadFromSource(ui, reload.sourceReference, result, changed);
        }
        else if (reload.type == UiAssetType::SerializedUi && reload.serializedAsset)
        {
            applied = applySerializedAsset(ui, *reload.serializedAsset, reload.sourcePath, nullptr, &result, &changed);
        }
        else if (reload.type == UiAssetType::WidgetAsset && reload.widgetAsset)
        {
            applied = applyWidgetAsset(ui, *reload.widgetAsset, reload.sourcePath, nullptr, &result, &changed);
        }
        else if (reload.type == UiAssetType::Theme && reload.theme)
        {
            applied = applyThemeAsset(ui, reload.themeId, *reload.theme, reload.dependencies, reload.sourcePath, &result, &changed);
        }

        result.success = result.success && applied;
    }

    rebuildAffectedConsumers(ui, changed, result);
    lastEvents = result.events;
    return result;
}

bool UiAssetRuntime::trackConsumer(const UiAssetConsumerDesc& desc,
                                   const UiSerializedLoadResult* loadResult)
{
    if (desc.id.empty() || !desc.enabled)
        return false;

    UiAssetConsumerState state;
    state.desc = desc;
    if (loadResult != nullptr)
        state.loadResult = *loadResult;
    const auto existing = consumers.find(desc.id);
    state.rebuildCount = existing == consumers.end() ? 0 : existing->second.rebuildCount;
    state.lastReloadVersion = existing == consumers.end() ? 0 : existing->second.lastReloadVersion;
    rebuildConsumerDependencies(state);
    consumers[state.desc.id] = std::move(state);
    return true;
}

bool UiAssetRuntime::unregisterConsumer(const std::string& consumerId)
{
    return consumers.erase(consumerId) > 0;
}

void UiAssetRuntime::unregisterConsumersForMount(UiSurfaceId surface, UiMountId mount)
{
    for (auto it = consumers.begin(); it != consumers.end();)
    {
        const UiAssetConsumerDesc& desc = it->second.desc;
        if (desc.surface == surface && desc.mount == mount)
            it = consumers.erase(it);
        else
            ++it;
    }
}

void UiAssetRuntime::unregisterConsumersForSurface(UiSurfaceId surface)
{
    for (auto it = consumers.begin(); it != consumers.end();)
    {
        if (it->second.desc.surface == surface)
            it = consumers.erase(it);
        else
            ++it;
    }
}

void UiAssetRuntime::clearConsumers()
{
    consumers.clear();
}

const UiAssetRecord* UiAssetRuntime::findAsset(const UiAssetReference& reference) const
{
    const auto it = records.find(uiAssetKey(reference));
    return it == records.end() ? nullptr : &it->second;
}

const UiAssetConsumerState* UiAssetRuntime::findConsumer(const std::string& consumerId) const
{
    const auto it = consumers.find(consumerId);
    return it == consumers.end() ? nullptr : &it->second;
}

std::vector<std::string> UiAssetRuntime::consumerIds() const
{
    std::vector<std::string> ids;
    ids.reserve(consumers.size());
    for (const auto& [id, _] : consumers)
        ids.push_back(id);
    std::sort(ids.begin(), ids.end());
    return ids;
}

std::vector<UiAssetReference> UiAssetRuntime::dependenciesOf(const UiAssetReference& reference, bool transitive) const
{
    std::vector<UiAssetReference> result;
    std::vector<UiAssetReference> stack;
    const UiAssetRecord* record = findAsset(reference);
    if (record != nullptr)
        stack = record->dependencies;

    while (!stack.empty())
    {
        UiAssetReference current = stack.back();
        stack.pop_back();
        if (!current.valid() || containsReference(result, current))
            continue;

        result.push_back(current);
        if (transitive)
        {
            const UiAssetRecord* dependency = findAsset(current);
            if (dependency != nullptr)
                stack.insert(stack.end(), dependency->dependencies.begin(), dependency->dependencies.end());
        }
    }

    std::sort(result.begin(), result.end(), [](const UiAssetReference& lhs, const UiAssetReference& rhs)
    {
        return uiAssetKey(lhs) < uiAssetKey(rhs);
    });
    return result;
}

std::vector<UiAssetReference> UiAssetRuntime::dependentsOf(const UiAssetReference& reference, bool transitive) const
{
    std::vector<UiAssetReference> result;
    std::vector<UiAssetReference> stack;
    const auto it = reverseDependencies.find(uiAssetKey(reference));
    if (it != reverseDependencies.end())
        stack = it->second;

    while (!stack.empty())
    {
        UiAssetReference current = stack.back();
        stack.pop_back();
        if (!current.valid() || containsReference(result, current))
            continue;

        result.push_back(current);
        if (transitive)
        {
            const auto depIt = reverseDependencies.find(uiAssetKey(current));
            if (depIt != reverseDependencies.end())
                stack.insert(stack.end(), depIt->second.begin(), depIt->second.end());
        }
    }

    std::sort(result.begin(), result.end(), [](const UiAssetReference& lhs, const UiAssetReference& rhs)
    {
        return uiAssetKey(lhs) < uiAssetKey(rhs);
    });
    return result;
}

std::vector<UiAssetReloadEvent> UiAssetRuntime::drainEvents()
{
    std::vector<UiAssetReloadEvent> drained = std::move(lastEvents);
    lastEvents.clear();
    return drained;
}

bool UiAssetRuntime::applySerializedAsset(UiSystem& ui,
                                          const UiSerializedAsset& asset,
                                          const std::filesystem::path& sourcePath,
                                          UiSerializedValidationResult* outValidation,
                                          UiAssetReloadResult* result,
                                          std::vector<UiAssetReference>* changed)
{
    UiSerializedValidationResult validation = validateUiSerializedAsset(asset, &ui.theme(), &ui.widgetAssets());
    if (outValidation != nullptr)
        *outValidation = validation;
    if (!validation.valid())
    {
        if (result != nullptr)
        {
            result->success = false;
            result->events.push_back(makeReloadEvent(UiAssetReloadEventKind::AssetFailedValidation,
                                                     UiAssetReference{UiAssetType::SerializedUi, asset.id},
                                                     {},
                                                     "serialized UI asset failed validation",
                                                     validation));
        }
        return false;
    }

    UiAssetRecord record;
    record.reference = UiAssetReference{UiAssetType::SerializedUi, asset.id};
    record.authoringVersion = asset.schemaVersion;
    record.sourcePath = sourcePath;
    record.state = UiAssetLoadState::Loaded;
    record.validation = validation;
    record.dependencies = collectSerializedAssetDependencies(asset);
    record.serializedAsset = asset;
    upsertRecord(std::move(record), result, changed);
    return true;
}

bool UiAssetRuntime::applyWidgetAsset(UiSystem& ui,
                                      const UiWidgetAssetDefinition& asset,
                                      const std::filesystem::path& sourcePath,
                                      UiSerializedValidationResult* outValidation,
                                      UiAssetReloadResult* result,
                                      std::vector<UiAssetReference>* changed)
{
    UiSerializedValidationResult validation;
    const bool registered = ui.widgetAssets().registerAsset(asset, &validation);
    if (outValidation != nullptr)
        *outValidation = validation;
    if (!registered || !validation.valid())
    {
        if (result != nullptr)
        {
            result->success = false;
            result->events.push_back(makeReloadEvent(UiAssetReloadEventKind::AssetFailedValidation,
                                                     UiAssetReference{UiAssetType::WidgetAsset, asset.id},
                                                     {},
                                                     "widget asset failed validation",
                                                     validation));
        }
        return false;
    }

    UiAssetRecord record;
    record.reference = UiAssetReference{UiAssetType::WidgetAsset, asset.id};
    record.authoringVersion = asset.version;
    record.sourcePath = sourcePath;
    record.state = UiAssetLoadState::Loaded;
    record.validation = validation;
    record.dependencies = collectWidgetAssetDependencies(asset);
    record.widgetAsset = asset;
    upsertRecord(std::move(record), result, changed);
    return true;
}

bool UiAssetRuntime::applyThemeAsset(UiSystem&,
                                     const std::string& id,
                                     const UiTheme& theme,
                                     const std::vector<UiAssetReference>& dependencies,
                                     const std::filesystem::path& sourcePath,
                                     UiAssetReloadResult* result,
                                     std::vector<UiAssetReference>* changed)
{
    if (id.empty())
        return false;

    UiAssetRecord record;
    record.reference = UiAssetReference{UiAssetType::Theme, id};
    record.authoringVersion = 1;
    record.sourcePath = sourcePath;
    record.state = UiAssetLoadState::Loaded;
    record.dependencies = dependencies;
    record.theme = theme;
    upsertRecord(std::move(record), result, changed);
    return true;
}

bool UiAssetRuntime::applyReloadFromSource(UiSystem& ui,
                                           const UiAssetReference& reference,
                                           UiAssetReloadResult& result,
                                           std::vector<UiAssetReference>& changed)
{
    const UiAssetRecord* existing = findAsset(reference);
    if (existing == nullptr || existing->sourcePath.empty())
    {
        result.success = false;
        result.events.push_back(makeReloadEvent(UiAssetReloadEventKind::AssetFailedValidation,
                                                reference,
                                                {},
                                                "asset has no source path to reload"));
        return false;
    }

    const std::string source = readTextFile(existing->sourcePath);
    if (source.empty())
    {
        result.success = false;
        result.events.push_back(makeReloadEvent(UiAssetReloadEventKind::AssetFailedValidation,
                                                reference,
                                                {},
                                                "asset source could not be read"));
        return false;
    }

    if (reference.type == UiAssetType::SerializedUi)
    {
        UiSerializedAsset asset;
        UiSerializedValidationResult validation;
        if (!parseUiSerializedAsset(source, asset, &validation))
        {
            result.success = false;
            result.events.push_back(makeReloadEvent(UiAssetReloadEventKind::AssetFailedValidation,
                                                    reference,
                                                    {},
                                                    "serialized UI source failed to parse",
                                                    validation));
            return false;
        }
        return applySerializedAsset(ui, asset, existing->sourcePath, nullptr, &result, &changed);
    }

    if (reference.type == UiAssetType::WidgetAsset)
    {
        UiWidgetAssetDefinition asset;
        UiSerializedValidationResult validation;
        if (!parseUiWidgetAssetDefinition(source, asset, &validation))
        {
            result.success = false;
            result.events.push_back(makeReloadEvent(UiAssetReloadEventKind::AssetFailedValidation,
                                                    reference,
                                                    {},
                                                    "widget asset source failed to parse",
                                                    validation));
            return false;
        }
        return applyWidgetAsset(ui, asset, existing->sourcePath, nullptr, &result, &changed);
    }

    result.success = false;
    result.events.push_back(makeReloadEvent(UiAssetReloadEventKind::AssetFailedValidation,
                                            reference,
                                            {},
                                            "asset type cannot be reloaded from source yet"));
    return false;
}

void UiAssetRuntime::upsertRecord(UiAssetRecord record,
                                  UiAssetReloadResult* result,
                                  std::vector<UiAssetReference>* changed)
{
    const std::string key = uiAssetKey(record.reference);
    const bool existed = records.find(key) != records.end();
    record.runtimeVersion = nextRuntimeVersion++;
    records[key] = std::move(record);
    rebuildReverseDependencies();

    if (changed != nullptr)
        appendUnique(*changed, records.at(key).reference);

    if (result != nullptr)
    {
        result->events.push_back(makeReloadEvent(existed ? UiAssetReloadEventKind::AssetReloaded
                                                         : UiAssetReloadEventKind::AssetLoaded,
                                                 records.at(key).reference,
                                                 {},
                                                 {},
                                                 {},
                                                 records.at(key).runtimeVersion));
        result->events.push_back(makeReloadEvent(UiAssetReloadEventKind::DependenciesChanged,
                                                 records.at(key).reference,
                                                 {},
                                                 {},
                                                 {},
                                                 records.at(key).runtimeVersion));
    }
}

void UiAssetRuntime::rebuildReverseDependencies()
{
    reverseDependencies.clear();
    for (const auto& [_, record] : records)
    {
        for (const UiAssetReference& dependency : record.dependencies)
            reverseDependencies[uiAssetKey(dependency)].push_back(record.reference);
    }
}

void UiAssetRuntime::rebuildConsumerDependencies(UiAssetConsumerState& consumer) const
{
    consumer.dependencies = dependenciesForConsumer(consumer.desc);
}

std::vector<UiAssetReference> UiAssetRuntime::dependenciesForConsumer(const UiAssetConsumerDesc& desc) const
{
    std::vector<UiAssetReference> refs;
    if (desc.kind == UiAssetConsumerKind::WidgetAsset && !desc.widgetAsset.assetId.empty())
    {
        UiAssetReference root{UiAssetType::WidgetAsset, desc.widgetAsset.assetId};
        appendUnique(refs, root);
        appendUnique(refs, dependenciesOf(root, true));
    }
    else if (desc.kind == UiAssetConsumerKind::SerializedUi)
    {
        if (!desc.serializedAsset.id.empty())
        {
            UiAssetReference root{UiAssetType::SerializedUi, desc.serializedAsset.id};
            appendUnique(refs, root);
            appendUnique(refs, dependenciesOf(root, true));
        }
        appendUnique(refs, collectSerializedAssetDependencies(desc.serializedAsset));
        std::vector<UiAssetReference> direct = refs;
        for (const UiAssetReference& ref : direct)
            appendUnique(refs, dependenciesOf(ref, true));
    }
    else if (desc.kind == UiAssetConsumerKind::SurfaceTheme && !desc.themeAssetId.empty())
    {
        UiAssetReference root{UiAssetType::Theme, desc.themeAssetId};
        appendUnique(refs, root);
        appendUnique(refs, dependenciesOf(root, true));
    }
    return refs;
}

bool UiAssetRuntime::rebuildConsumer(UiSystem& ui, const std::string& consumerId, UiAssetReloadResult& result)
{
    auto it = consumers.find(consumerId);
    if (it == consumers.end())
        return false;

    UiAssetConsumerState previous = it->second;
    if (!previous.desc.enabled)
        return true;

    if (previous.desc.kind == UiAssetConsumerKind::SurfaceTheme)
    {
        const UiAssetRecord* record = findAsset(UiAssetReference{UiAssetType::Theme, previous.desc.themeAssetId});
        if (record == nullptr || !record->theme)
            return false;

        const bool applied = ui.setTheme(previous.desc.surface, *record->theme);
        if (!applied)
        {
            result.failedConsumers++;
            result.success = false;
            result.events.push_back(makeReloadEvent(UiAssetReloadEventKind::ConsumerFailed,
                                                    record->reference,
                                                    consumerId,
                                                    "surface theme consumer could not apply theme"));
            return false;
        }

        previous.rebuildCount++;
        previous.lastReloadVersion = record->runtimeVersion;
        rebuildConsumerDependencies(previous);
        consumers[consumerId] = std::move(previous);
        result.rebuiltConsumers++;
        result.events.push_back(makeReloadEvent(UiAssetReloadEventKind::ConsumerRebuilt,
                                                record->reference,
                                                consumerId,
                                                {},
                                                {},
                                                record->runtimeVersion));
        return true;
    }

    const std::string focusedId = focusedWidgetId(ui, previous);
    if (previous.desc.mount != UI_INVALID_MOUNT)
        ui.destroyMount(previous.desc.surface, previous.desc.mount);

    UiMountDesc mountDesc = previous.desc.mountDesc;
    if (mountDesc.name.empty())
        mountDesc.name = "Live UI Asset: " + consumerId;
    previous.desc.mount = ui.createMount(previous.desc.surface, mountDesc);
    if (previous.desc.mount == UI_INVALID_MOUNT)
    {
        previous.loadResult = {};
        previous.loadResult.validation.error("reload", "consumer mount could not be recreated");
        consumers[consumerId] = previous;
        result.failedConsumers++;
        result.success = false;
        result.events.push_back(makeReloadEvent(UiAssetReloadEventKind::ConsumerFailed,
                                                {},
                                                consumerId,
                                                "consumer mount could not be recreated",
                                                previous.loadResult.validation));
        return false;
    }

    if (previous.desc.kind == UiAssetConsumerKind::WidgetAsset)
    {
        previous.loadResult = ui.widgetAssets().instantiate(ui,
                                                            previous.desc.surface,
                                                            previous.desc.mount,
                                                            previous.desc.widgetAsset,
                                                            previous.desc.bindingResolver,
                                                            ui.theme(previous.desc.surface));
    }
    else
    {
        previous.loadResult = UiSerializationRuntime::instantiate(ui,
                                                                  previous.desc.surface,
                                                                  previous.desc.mount,
                                                                  previous.desc.serializedAsset,
                                                                  previous.desc.bindingResolver,
                                                                  ui.theme(previous.desc.surface),
                                                                  &ui.widgetAssets());
    }

    previous.rebuildCount++;
    previous.lastReloadVersion = nextRuntimeVersion - 1;
    rebuildConsumerDependencies(previous);
    const bool loaded = previous.loadResult.success;
    if (loaded && previous.desc.preserveFocus)
        restoreFocusedWidget(ui, previous, focusedId);

    consumers[consumerId] = previous;
    if (loaded)
    {
        result.rebuiltConsumers++;
        result.events.push_back(makeReloadEvent(UiAssetReloadEventKind::ConsumerRebuilt,
                                                {},
                                                consumerId,
                                                {},
                                                {},
                                                previous.lastReloadVersion));
    }
    else
    {
        result.failedConsumers++;
        result.success = false;
        result.events.push_back(makeReloadEvent(UiAssetReloadEventKind::ConsumerFailed,
                                                {},
                                                consumerId,
                                                "consumer failed to instantiate reloaded asset",
                                                previous.loadResult.validation));
    }
    return loaded;
}

void UiAssetRuntime::rebuildAffectedConsumers(UiSystem& ui,
                                              const std::vector<UiAssetReference>& changed,
                                              UiAssetReloadResult& result)
{
    if (changed.empty())
        return;

    const std::vector<UiAssetReference> invalidated = collectInvalidatedAssets(changed);
    result.invalidatedAssets = invalidated;
    for (const UiAssetReference& ref : invalidated)
    {
        result.events.push_back(makeReloadEvent(UiAssetReloadEventKind::AssetInvalidated, ref));
    }

    std::vector<std::string> affectedConsumers;
    for (auto& [consumerId, consumer] : consumers)
    {
        rebuildConsumerDependencies(consumer);
        for (const UiAssetReference& dependency : consumer.dependencies)
        {
            if (containsReference(invalidated, dependency))
            {
                affectedConsumers.push_back(consumerId);
                result.events.push_back(makeReloadEvent(UiAssetReloadEventKind::ConsumerInvalidated,
                                                        dependency,
                                                        consumerId));
                break;
            }
        }
    }

    std::sort(affectedConsumers.begin(), affectedConsumers.end());
    affectedConsumers.erase(std::unique(affectedConsumers.begin(), affectedConsumers.end()), affectedConsumers.end());
    for (const std::string& consumerId : affectedConsumers)
        rebuildConsumer(ui, consumerId, result);
}

std::vector<UiAssetReference> UiAssetRuntime::collectInvalidatedAssets(const std::vector<UiAssetReference>& changed) const
{
    std::vector<UiAssetReference> result;
    for (const UiAssetReference& ref : changed)
    {
        appendUnique(result, ref);
        appendUnique(result, dependentsOf(ref, true));
    }
    std::sort(result.begin(), result.end(), [](const UiAssetReference& lhs, const UiAssetReference& rhs)
    {
        return uiAssetKey(lhs) < uiAssetKey(rhs);
    });
    return result;
}

std::string UiAssetRuntime::focusedWidgetId(UiSystem& ui, const UiAssetConsumerState& consumer) const
{
    if (!consumer.desc.preserveFocus)
        return {};

    UiSurface* surface = ui.findSurface(consumer.desc.surface);
    if (surface == nullptr)
        return {};

    const UiHandle focused = surface->focusManager().focusedNode();
    if (focused == UI_INVALID_HANDLE)
        return {};

    for (const auto& [widgetId, handle] : consumer.loadResult.instance.handles)
    {
        if (handle == focused)
            return widgetId;
    }
    return {};
}

void UiAssetRuntime::restoreFocusedWidget(UiSystem& ui,
                                          const UiAssetConsumerState& consumer,
                                          const std::string& widgetId) const
{
    if (widgetId.empty())
        return;

    const auto handleIt = consumer.loadResult.instance.handles.find(widgetId);
    if (handleIt == consumer.loadResult.instance.handles.end())
        return;

    UiSurface* surface = ui.findSurface(consumer.desc.surface);
    if (surface == nullptr)
        return;

    surface->focusManager().requestFocus(surface->document(), handleIt->second, UiFocusReason::Restore);
}
