#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "UiMount.h"
#include "UiSerialization.h"
#include "UiWidgetAsset.h"

class UiSystem;

enum class UiAssetType
{
    Unknown,
    SerializedUi,
    WidgetAsset,
    Theme
};

struct UiAssetReference
{
    UiAssetType type = UiAssetType::Unknown;
    std::string id;

    bool valid() const { return type != UiAssetType::Unknown && !id.empty(); }
    bool operator==(const UiAssetReference& rhs) const = default;
};

std::string uiAssetTypeName(UiAssetType type);
std::string uiAssetKey(const UiAssetReference& reference);
UiAssetReference uiAssetReferenceFromDependencyString(const std::string& dependency);
std::string uiWidgetAssetConsumerId(const UiWidgetAssetInstanceDesc& desc,
                                    UiSurfaceId surface,
                                    UiMountId mount);
std::string uiSerializedAssetConsumerId(const UiSerializedAsset& asset,
                                        UiSurfaceId surface,
                                        UiMountId mount);

enum class UiAssetLoadState
{
    Unloaded,
    Loaded,
    Invalid
};

struct UiAssetRecord
{
    UiAssetReference reference;
    int authoringVersion = 0;
    uint64_t runtimeVersion = 0;
    std::filesystem::path sourcePath;
    UiAssetLoadState state = UiAssetLoadState::Unloaded;
    UiSerializedValidationResult validation;
    std::vector<UiAssetReference> dependencies;

    std::optional<UiSerializedAsset> serializedAsset;
    std::optional<UiWidgetAssetDefinition> widgetAsset;
    std::optional<UiTheme> theme;
};

enum class UiAssetReloadEventKind
{
    AssetLoaded,
    AssetReloaded,
    AssetInvalidated,
    AssetFailedValidation,
    DependenciesChanged,
    ConsumerInvalidated,
    ConsumerRebuilt,
    ConsumerFailed
};

struct UiAssetReloadEvent
{
    UiAssetReloadEventKind kind = UiAssetReloadEventKind::AssetLoaded;
    UiAssetReference asset;
    std::string consumerId;
    std::string message;
    UiSerializedValidationResult validation;
    uint64_t runtimeVersion = 0;
};

struct UiAssetReloadResult
{
    bool success = true;
    std::vector<UiAssetReloadEvent> events;
    std::vector<UiAssetReference> invalidatedAssets;
    uint32_t rebuiltConsumers = 0;
    uint32_t failedConsumers = 0;
};

enum class UiAssetConsumerKind
{
    SerializedUi,
    WidgetAsset,
    SurfaceTheme
};

struct UiAssetConsumerDesc
{
    std::string id;
    UiAssetConsumerKind kind = UiAssetConsumerKind::WidgetAsset;
    UiSurfaceId surface = UI_INVALID_SURFACE;
    UiMountId mount = UI_INVALID_MOUNT;
    UiMountDesc mountDesc;
    UiSerializedAsset serializedAsset;
    UiWidgetAssetInstanceDesc widgetAsset;
    std::string themeAssetId;
    const IUiSerializedBindingResolver* bindingResolver = nullptr;
    bool preserveFocus = true;
    bool enabled = true;
};

struct UiAssetConsumerState
{
    UiAssetConsumerDesc desc;
    UiSerializedLoadResult loadResult;
    std::vector<UiAssetReference> dependencies;
    uint32_t rebuildCount = 0;
    uint64_t lastReloadVersion = 0;
};

class UiAssetRuntime
{
public:
    bool registerSerializedAsset(UiSystem& ui,
                                 const UiSerializedAsset& asset,
                                 UiSerializedValidationResult* outValidation = nullptr,
                                 std::filesystem::path sourcePath = {});
    bool registerWidgetAsset(UiSystem& ui,
                             const UiWidgetAssetDefinition& asset,
                             UiSerializedValidationResult* outValidation = nullptr,
                             std::filesystem::path sourcePath = {});
    bool registerThemeAsset(UiSystem& ui,
                            const std::string& id,
                            const UiTheme& theme,
                            std::vector<UiAssetReference> dependencies = {},
                            std::filesystem::path sourcePath = {});
    bool unregisterAsset(UiSystem& ui,
                         const UiAssetReference& reference,
                         UiAssetReloadResult* outResult = nullptr);

    bool queueSerializedAssetReload(const UiSerializedAsset& asset,
                                    std::filesystem::path sourcePath = {});
    bool queueWidgetAssetReload(const UiWidgetAssetDefinition& asset,
                                std::filesystem::path sourcePath = {});
    bool queueThemeReload(const std::string& id,
                          const UiTheme& theme,
                          std::vector<UiAssetReference> dependencies = {},
                          std::filesystem::path sourcePath = {});
    bool queueReloadFromSource(const UiAssetReference& reference);
    UiAssetReloadResult processReloads(UiSystem& ui);

    bool trackConsumer(const UiAssetConsumerDesc& desc,
                       const UiSerializedLoadResult* loadResult = nullptr);
    bool unregisterConsumer(const std::string& consumerId);
    void unregisterConsumersForMount(UiSurfaceId surface, UiMountId mount);
    void unregisterConsumersForSurface(UiSurfaceId surface);
    void clearConsumers();

    const UiAssetRecord* findAsset(const UiAssetReference& reference) const;
    const UiAssetConsumerState* findConsumer(const std::string& consumerId) const;
    std::vector<std::string> consumerIds() const;
    std::vector<UiAssetReference> dependenciesOf(const UiAssetReference& reference, bool transitive = false) const;
    std::vector<UiAssetReference> dependentsOf(const UiAssetReference& reference, bool transitive = false) const;

    const std::vector<UiAssetReloadEvent>& events() const { return lastEvents; }
    std::vector<UiAssetReloadEvent> drainEvents();

private:
    struct PendingReload
    {
        UiAssetType type = UiAssetType::Unknown;
        std::optional<UiSerializedAsset> serializedAsset;
        std::optional<UiWidgetAssetDefinition> widgetAsset;
        std::optional<UiTheme> theme;
        std::string themeId;
        std::vector<UiAssetReference> dependencies;
        std::filesystem::path sourcePath;
        UiAssetReference sourceReference;
        bool fromSource = false;
    };

    bool applySerializedAsset(UiSystem& ui,
                              const UiSerializedAsset& asset,
                              const std::filesystem::path& sourcePath,
                              UiSerializedValidationResult* outValidation,
                              UiAssetReloadResult* result,
                              std::vector<UiAssetReference>* changed);
    bool applyWidgetAsset(UiSystem& ui,
                          const UiWidgetAssetDefinition& asset,
                          const std::filesystem::path& sourcePath,
                          UiSerializedValidationResult* outValidation,
                          UiAssetReloadResult* result,
                          std::vector<UiAssetReference>* changed);
    bool applyThemeAsset(UiSystem& ui,
                         const std::string& id,
                         const UiTheme& theme,
                         const std::vector<UiAssetReference>& dependencies,
                         const std::filesystem::path& sourcePath,
                         UiAssetReloadResult* result,
                         std::vector<UiAssetReference>* changed);
    bool applyReloadFromSource(UiSystem& ui,
                               const UiAssetReference& reference,
                               UiAssetReloadResult& result,
                               std::vector<UiAssetReference>& changed);

    void upsertRecord(UiAssetRecord record, UiAssetReloadResult* result, std::vector<UiAssetReference>* changed);
    void rebuildReverseDependencies();
    void rebuildConsumerDependencies(UiAssetConsumerState& consumer) const;
    std::vector<UiAssetReference> dependenciesForConsumer(const UiAssetConsumerDesc& desc) const;
    bool rebuildConsumer(UiSystem& ui, const std::string& consumerId, UiAssetReloadResult& result);
    void rebuildAffectedConsumers(UiSystem& ui,
                                  const std::vector<UiAssetReference>& changed,
                                  UiAssetReloadResult& result);
    std::vector<UiAssetReference> collectInvalidatedAssets(const std::vector<UiAssetReference>& changed) const;
    std::string focusedWidgetId(UiSystem& ui, const UiAssetConsumerState& consumer) const;
    void restoreFocusedWidget(UiSystem& ui, const UiAssetConsumerState& consumer, const std::string& widgetId) const;

    std::unordered_map<std::string, UiAssetRecord> records;
    std::unordered_map<std::string, std::vector<UiAssetReference>> reverseDependencies;
    std::unordered_map<std::string, UiAssetConsumerState> consumers;
    std::vector<PendingReload> pendingReloads;
    std::vector<UiAssetReloadEvent> lastEvents;
    uint64_t nextRuntimeVersion = 1;
};
