#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "UiAssetRuntime.h"

inline constexpr int UI_LOCALIZATION_SCHEMA_VERSION = 1;

struct UiLocaleId
{
    std::string tag;

    bool empty() const { return tag.empty(); }
    bool operator==(const UiLocaleId&) const = default;
};

UiLocaleId parseUiLocaleId(std::string_view tag);
std::vector<UiLocaleId> uiLocaleFallbackChain(const UiLocaleId& locale);

struct UiLocalizationKey
{
    std::string packageId;
    std::string namespaceId;
    std::string key;

    bool empty() const { return key.empty(); }
};

struct UiLocalizationScope
{
    std::string packageId;
    std::string namespaceId;
};

struct UiLocalizedTextRef
{
    UiLocalizationKey key;
    std::string fallbackText;
    std::string context;
};

struct UiLocalizationEntry
{
    std::string text;
    std::string context;
    std::string translatorNote;
};

struct UiLocalizationAsset
{
    int schemaVersion = UI_LOCALIZATION_SCHEMA_VERSION;
    UiAssetReference asset;
    std::string packageId;
    std::string namespaceId;
    UiLocaleId locale;
    std::optional<UiLocaleId> sourceLocale;
    std::optional<UiLocaleId> fallbackLocale;
    std::unordered_map<std::string, UiLocalizationEntry> entries;
};

struct UiLocalizedString
{
    std::string text;
    UiLocalizationKey key;
    UiLocaleId locale;
    UiAssetReference asset;
    bool found = false;
    bool usedFallbackLocale = false;
    bool usedInlineFallback = false;
    bool missing = false;
    std::string diagnostic;
};

struct UiLocalizationDiagnostic
{
    UiLocalizationKey key;
    UiLocalizationScope scope;
    UiLocaleId locale;
    std::string message;
};

bool parseUiLocalizationAsset(const std::string& json,
                              UiLocalizationAsset& outAsset,
                              UiSerializedValidationResult* outValidation = nullptr);
std::string serializeUiLocalizationAsset(const UiLocalizationAsset& asset);

class UiLocalizationRuntime
{
public:
    struct LookupCandidate
    {
        std::string packageId;
        std::string namespaceId;
        std::string key;
    };

    bool registerCatalog(const UiLocalizationAsset& asset,
                         UiSerializedValidationResult* validation = nullptr);
    bool unregisterCatalog(const UiAssetReference& asset);
    void clear();

    void setLocale(UiLocaleId locale);
    void setLocale(std::string_view locale);
    UiLocaleId locale() const { return activeLocale; }

    void setDefaultLocale(UiLocaleId locale);
    void setDefaultLocale(std::string_view locale);
    UiLocaleId defaultLocale() const { return fallbackLocale; }

    UiLocalizedString resolve(const UiLocalizedTextRef& ref,
                              const UiLocalizationScope& scope = {}) const;
    UiLocalizedString resolveKey(const std::string& key,
                                 const UiLocalizationScope& scope = {},
                                 const std::string& fallbackText = {}) const;

    UiSerializedValidationResult validateCatalog(const UiLocalizationAsset& asset) const;
    const UiLocalizationAsset* findCatalog(const UiAssetReference& asset) const;
    std::vector<UiAssetReference> catalogAssets() const;

    const std::vector<UiLocalizationDiagnostic>& diagnostics() const { return lastDiagnostics; }
    std::vector<UiLocalizationDiagnostic> drainDiagnostics() const;

private:
    struct CatalogRecord
    {
        UiLocalizationAsset asset;
        uint64_t order = 0;
    };

    std::vector<LookupCandidate> lookupCandidates(const UiLocalizationKey& key,
                                                  const UiLocalizationScope& scope) const;
    std::vector<UiLocaleId> fallbackLocales(const std::vector<LookupCandidate>& candidates) const;
    const CatalogRecord* findEntry(const LookupCandidate& candidate,
                                   const UiLocaleId& locale,
                                   UiLocalizationEntry& outEntry) const;
    bool catalogMatchesCandidate(const UiLocalizationAsset& asset,
                                 const LookupCandidate& candidate) const;
    void appendKnownNamespaceCandidates(const std::string& key,
                                        std::vector<LookupCandidate>& candidates) const;
    void addDiagnostic(const UiLocalizationKey& key,
                       const UiLocalizationScope& scope,
                       std::string message) const;

    std::unordered_map<std::string, CatalogRecord> catalogs;
    UiLocaleId activeLocale = parseUiLocaleId("en-US");
    UiLocaleId fallbackLocale = parseUiLocaleId("en");
    uint64_t nextOrder = 1;
    mutable std::vector<UiLocalizationDiagnostic> lastDiagnostics;
};
