#include "UiLocalizationRuntime.h"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <unordered_set>

namespace
{
    std::string trim(std::string_view value)
    {
        size_t begin = 0;
        while (begin < value.size() && std::isspace(static_cast<unsigned char>(value[begin])) != 0)
            ++begin;

        size_t end = value.size();
        while (end > begin && std::isspace(static_cast<unsigned char>(value[end - 1])) != 0)
            --end;

        return std::string(value.substr(begin, end - begin));
    }

    std::vector<std::string> splitLocale(std::string_view value)
    {
        std::vector<std::string> parts;
        size_t start = 0;
        while (start <= value.size())
        {
            const size_t sep = value.find('-', start);
            if (sep == std::string_view::npos)
            {
                parts.emplace_back(value.substr(start));
                break;
            }
            parts.emplace_back(value.substr(start, sep - start));
            start = sep + 1;
        }
        return parts;
    }

    std::string lowerAscii(std::string value)
    {
        for (char& ch : value)
            ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        return value;
    }

    std::string upperAscii(std::string value)
    {
        for (char& ch : value)
            ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
        return value;
    }

    std::string titleAscii(std::string value)
    {
        value = lowerAscii(std::move(value));
        if (!value.empty())
            value[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(value[0])));
        return value;
    }

    void appendLocale(std::vector<UiLocaleId>& locales, UiLocaleId locale)
    {
        if (locale.tag.empty())
            return;
        if (std::find(locales.begin(), locales.end(), locale) == locales.end())
            locales.push_back(std::move(locale));
    }

    void appendLocaleWithParents(std::vector<UiLocaleId>& locales, const UiLocaleId& locale)
    {
        for (const UiLocaleId& candidate : uiLocaleFallbackChain(locale))
            appendLocale(locales, candidate);
    }

    void appendCandidate(std::vector<UiLocalizationRuntime::LookupCandidate>& candidates,
                         UiLocalizationRuntime::LookupCandidate candidate)
    {
        if (candidate.key.empty())
            return;

        const auto it = std::find_if(candidates.begin(),
                                     candidates.end(),
                                     [&candidate](const UiLocalizationRuntime::LookupCandidate& existing)
                                     {
                                         return existing.packageId == candidate.packageId &&
                                                existing.namespaceId == candidate.namespaceId &&
                                                existing.key == candidate.key;
                                     });
        if (it == candidates.end())
            candidates.push_back(std::move(candidate));
    }

    const UiJsonValue* findField(const UiJsonValue& value, const std::string& key)
    {
        return value.find(key);
    }

    std::string jsonStringValue(const UiJsonValue* value, const std::string& fallback = {})
    {
        if (value == nullptr || !value->isString())
            return fallback;
        return std::get<std::string>(value->value);
    }

    int jsonIntValue(const UiJsonValue* value, int fallback = 0)
    {
        if (value == nullptr || !value->isNumber())
            return fallback;
        return static_cast<int>(std::get<double>(value->value));
    }

    UiJsonValue jsonString(std::string value)
    {
        UiJsonValue json;
        json.value = std::move(value);
        return json;
    }

    UiJsonValue jsonNumber(double value)
    {
        UiJsonValue json;
        json.value = value;
        return json;
    }

    UiJsonValue jsonObject(UiJsonValue::Object values)
    {
        UiJsonValue json;
        json.value = std::move(values);
        return json;
    }

    bool hasBalancedPlaceholders(const std::string& text)
    {
        bool open = false;
        for (const char ch : text)
        {
            if (ch == '{')
            {
                if (open)
                    return false;
                open = true;
            }
            else if (ch == '}')
            {
                if (!open)
                    return false;
                open = false;
            }
        }
        return !open;
    }

    std::string catalogKey(const UiLocalizationAsset& asset)
    {
        return uiAssetKey(asset.asset);
    }

    std::string placeholderFor(const UiLocalizationKey& key)
    {
        std::string prefix;
        if (!key.namespaceId.empty())
            prefix = key.namespaceId + ".";
        else if (!key.packageId.empty())
            prefix = key.packageId + ".";
        return "!" + prefix + key.key + "!";
    }
}

UiLocaleId parseUiLocaleId(std::string_view tag)
{
    std::string normalized = trim(tag);
    std::replace(normalized.begin(), normalized.end(), '_', '-');
    if (normalized.empty())
        return {};

    std::vector<std::string> parts = splitLocale(normalized);
    for (size_t i = 0; i < parts.size(); ++i)
    {
        if (i == 0)
            parts[i] = lowerAscii(std::move(parts[i]));
        else if (parts[i].size() == 2 || parts[i].size() == 3)
            parts[i] = upperAscii(std::move(parts[i]));
        else if (parts[i].size() == 4)
            parts[i] = titleAscii(std::move(parts[i]));
        else
            parts[i] = lowerAscii(std::move(parts[i]));
    }

    std::ostringstream joined;
    for (size_t i = 0; i < parts.size(); ++i)
    {
        if (i > 0)
            joined << '-';
        joined << parts[i];
    }
    return UiLocaleId{joined.str()};
}

std::vector<UiLocaleId> uiLocaleFallbackChain(const UiLocaleId& locale)
{
    std::vector<UiLocaleId> result;
    UiLocaleId current = parseUiLocaleId(locale.tag);
    while (!current.tag.empty())
    {
        appendLocale(result, current);
        const size_t sep = current.tag.rfind('-');
        if (sep == std::string::npos)
            break;
        current = parseUiLocaleId(std::string_view(current.tag).substr(0, sep));
    }
    return result;
}

bool parseUiLocalizationAsset(const std::string& json,
                              UiLocalizationAsset& outAsset,
                              UiSerializedValidationResult* outValidation)
{
    UiSerializedValidationResult validation;
    UiJsonValue root;
    std::string parseError;
    if (!parseUiJson(json, root, &parseError))
    {
        validation.error("$", parseError.empty() ? "localization asset JSON parse failed" : parseError);
        if (outValidation != nullptr)
            *outValidation = validation;
        return false;
    }

    if (!root.isObject())
    {
        validation.error("$", "localization asset root must be an object");
        if (outValidation != nullptr)
            *outValidation = validation;
        return false;
    }

    UiLocalizationAsset asset;
    asset.schemaVersion = jsonIntValue(findField(root, "schema"), UI_LOCALIZATION_SCHEMA_VERSION);
    asset.asset = UiAssetReference{UiAssetType::Localization, jsonStringValue(findField(root, "id"))};
    asset.packageId = jsonStringValue(findField(root, "package"));
    asset.namespaceId = jsonStringValue(findField(root, "namespace"));
    asset.locale = parseUiLocaleId(jsonStringValue(findField(root, "locale")));

    const std::string sourceLocale = jsonStringValue(findField(root, "sourceLocale"));
    if (!sourceLocale.empty())
        asset.sourceLocale = parseUiLocaleId(sourceLocale);
    const std::string fallbackLocale = jsonStringValue(findField(root, "fallbackLocale"));
    if (!fallbackLocale.empty())
        asset.fallbackLocale = parseUiLocaleId(fallbackLocale);

    const UiJsonValue* entries = findField(root, "entries");
    if (entries == nullptr || !entries->isObject())
    {
        validation.error("$.entries", "localization asset entries object is required");
    }
    else
    {
        std::unordered_set<std::string> seenKeys;
        for (const auto& [key, value] : std::get<UiJsonValue::Object>(entries->value))
        {
            const std::string path = "$.entries." + key;
            if (!seenKeys.insert(key).second)
            {
                validation.error(path, "duplicate localization key");
                continue;
            }

            UiLocalizationEntry entry;
            if (value.isString())
            {
                entry.text = std::get<std::string>(value.value);
            }
            else if (value.isObject())
            {
                entry.text = jsonStringValue(findField(value, "text"));
                entry.context = jsonStringValue(findField(value, "context"));
                entry.translatorNote = jsonStringValue(findField(value, "note"));
            }
            else
            {
                validation.error(path, "localization entry must be a string or object");
                continue;
            }

            asset.entries[key] = std::move(entry);
        }
    }

    UiLocalizationRuntime runtime;
    UiSerializedValidationResult assetValidation = runtime.validateCatalog(asset);
    validation.issues.insert(validation.issues.end(),
                             assetValidation.issues.begin(),
                             assetValidation.issues.end());

    if (outValidation != nullptr)
        *outValidation = validation;
    if (!validation.valid())
        return false;

    outAsset = std::move(asset);
    return true;
}

std::string serializeUiLocalizationAsset(const UiLocalizationAsset& asset)
{
    UiJsonValue::Object root;
    root.emplace_back("schema", jsonNumber(asset.schemaVersion));
    root.emplace_back("id", jsonString(asset.asset.id));
    if (!asset.packageId.empty())
        root.emplace_back("package", jsonString(asset.packageId));
    if (!asset.namespaceId.empty())
        root.emplace_back("namespace", jsonString(asset.namespaceId));
    root.emplace_back("locale", jsonString(asset.locale.tag));
    if (asset.sourceLocale)
        root.emplace_back("sourceLocale", jsonString(asset.sourceLocale->tag));
    if (asset.fallbackLocale)
        root.emplace_back("fallbackLocale", jsonString(asset.fallbackLocale->tag));

    std::vector<std::string> keys;
    keys.reserve(asset.entries.size());
    for (const auto& [key, _] : asset.entries)
        keys.push_back(key);
    std::sort(keys.begin(), keys.end());

    UiJsonValue::Object entries;
    for (const std::string& key : keys)
    {
        const UiLocalizationEntry& entry = asset.entries.at(key);
        UiJsonValue::Object entryObject;
        entryObject.emplace_back("text", jsonString(entry.text));
        if (!entry.context.empty())
            entryObject.emplace_back("context", jsonString(entry.context));
        if (!entry.translatorNote.empty())
            entryObject.emplace_back("note", jsonString(entry.translatorNote));
        entries.emplace_back(key, jsonObject(std::move(entryObject)));
    }
    root.emplace_back("entries", jsonObject(std::move(entries)));
    return serializeUiJson(jsonObject(std::move(root)));
}

bool UiLocalizationRuntime::registerCatalog(const UiLocalizationAsset& asset,
                                            UiSerializedValidationResult* validation)
{
    UiSerializedValidationResult localValidation = validateCatalog(asset);
    if (validation != nullptr)
        *validation = localValidation;
    if (!localValidation.valid())
        return false;

    CatalogRecord record;
    record.asset = asset;
    record.asset.locale = parseUiLocaleId(record.asset.locale.tag);
    if (record.asset.sourceLocale)
        record.asset.sourceLocale = parseUiLocaleId(record.asset.sourceLocale->tag);
    if (record.asset.fallbackLocale)
        record.asset.fallbackLocale = parseUiLocaleId(record.asset.fallbackLocale->tag);
    record.order = nextOrder++;
    catalogs[catalogKey(record.asset)] = std::move(record);
    return true;
}

bool UiLocalizationRuntime::unregisterCatalog(const UiAssetReference& asset)
{
    if (asset.type != UiAssetType::Localization)
        return false;
    return catalogs.erase(uiAssetKey(asset)) > 0;
}

void UiLocalizationRuntime::clear()
{
    catalogs.clear();
    lastDiagnostics.clear();
    nextOrder = 1;
}

void UiLocalizationRuntime::setLocale(UiLocaleId locale)
{
    activeLocale = parseUiLocaleId(locale.tag);
}

void UiLocalizationRuntime::setLocale(std::string_view locale)
{
    setLocale(parseUiLocaleId(locale));
}

void UiLocalizationRuntime::setDefaultLocale(UiLocaleId locale)
{
    fallbackLocale = parseUiLocaleId(locale.tag);
}

void UiLocalizationRuntime::setDefaultLocale(std::string_view locale)
{
    setDefaultLocale(parseUiLocaleId(locale));
}

UiLocalizedString UiLocalizationRuntime::resolve(const UiLocalizedTextRef& ref,
                                                 const UiLocalizationScope& scope) const
{
    UiLocalizedString result;
    result.key = ref.key;
    result.locale = activeLocale;

    if (ref.key.key.empty())
    {
        result.text = ref.fallbackText.empty() ? "!!" : ref.fallbackText;
        result.usedInlineFallback = !ref.fallbackText.empty();
        result.missing = true;
        result.diagnostic = "localization key is empty";
        addDiagnostic(ref.key, scope, result.diagnostic);
        return result;
    }

    const std::vector<LookupCandidate> candidates = lookupCandidates(ref.key, scope);
    const std::vector<UiLocaleId> locales = fallbackLocales(candidates);
    for (const UiLocaleId& localeCandidate : locales)
    {
        for (const LookupCandidate& candidate : candidates)
        {
            UiLocalizationEntry entry;
            const CatalogRecord* catalog = findEntry(candidate, localeCandidate, entry);
            if (catalog == nullptr)
                continue;

            result.text = entry.text;
            result.locale = localeCandidate;
            result.asset = catalog->asset.asset;
            result.key.packageId = candidate.packageId;
            result.key.namespaceId = candidate.namespaceId;
            result.key.key = candidate.key;
            result.found = true;
            result.usedFallbackLocale = localeCandidate != activeLocale;
            return result;
        }
    }

    result.missing = true;
    result.diagnostic = "missing localization key '" + ref.key.key + "' for locale '" + activeLocale.tag + "'";
    if (!ref.fallbackText.empty())
    {
        result.text = ref.fallbackText;
        result.usedInlineFallback = true;
    }
    else
    {
        result.text = placeholderFor(ref.key);
    }
    addDiagnostic(ref.key, scope, result.diagnostic);
    return result;
}

UiLocalizedString UiLocalizationRuntime::resolveKey(const std::string& key,
                                                    const UiLocalizationScope& scope,
                                                    const std::string& fallbackText) const
{
    UiLocalizedTextRef ref;
    ref.key.key = key;
    ref.fallbackText = fallbackText;
    return resolve(ref, scope);
}

UiSerializedValidationResult UiLocalizationRuntime::validateCatalog(const UiLocalizationAsset& asset) const
{
    UiSerializedValidationResult validation;
    if (asset.schemaVersion != UI_LOCALIZATION_SCHEMA_VERSION)
        validation.error("$.schema", "unsupported localization asset schema");
    if (asset.asset.type != UiAssetType::Localization || asset.asset.id.empty())
        validation.error("$.id", "localization asset id is required");
    if (asset.packageId.empty())
        validation.error("$.package", "localization asset package is required");
    if (asset.namespaceId.empty())
        validation.error("$.namespace", "localization asset namespace is required");
    if (parseUiLocaleId(asset.locale.tag).tag.empty())
        validation.error("$.locale", "localization asset locale is required");
    if (asset.fallbackLocale && parseUiLocaleId(asset.fallbackLocale->tag) == parseUiLocaleId(asset.locale.tag))
        validation.warning("$.fallbackLocale", "fallback locale matches catalog locale");
    if (asset.entries.empty())
        validation.warning("$.entries", "localization asset contains no entries");

    for (const auto& [key, entry] : asset.entries)
    {
        const std::string path = "$.entries." + key;
        if (key.empty())
            validation.error(path, "localization key must not be empty");
        if (entry.text.empty())
            validation.warning(path + ".text", "localization entry text is empty");
        if (!hasBalancedPlaceholders(entry.text))
            validation.error(path + ".text", "localization entry has unbalanced placeholder braces");
    }
    return validation;
}

const UiLocalizationAsset* UiLocalizationRuntime::findCatalog(const UiAssetReference& asset) const
{
    const auto it = catalogs.find(uiAssetKey(asset));
    return it == catalogs.end() ? nullptr : &it->second.asset;
}

std::vector<UiAssetReference> UiLocalizationRuntime::catalogAssets() const
{
    std::vector<UiAssetReference> result;
    result.reserve(catalogs.size());
    for (const auto& [_, record] : catalogs)
        result.push_back(record.asset.asset);
    std::sort(result.begin(),
              result.end(),
              [](const UiAssetReference& lhs, const UiAssetReference& rhs)
              {
                  return uiAssetKey(lhs) < uiAssetKey(rhs);
              });
    return result;
}

std::vector<UiLocalizationDiagnostic> UiLocalizationRuntime::drainDiagnostics() const
{
    std::vector<UiLocalizationDiagnostic> result = std::move(lastDiagnostics);
    lastDiagnostics.clear();
    return result;
}

std::vector<UiLocalizationRuntime::LookupCandidate>
UiLocalizationRuntime::lookupCandidates(const UiLocalizationKey& key,
                                        const UiLocalizationScope& scope) const
{
    std::vector<LookupCandidate> candidates;
    if (key.key.empty())
        return candidates;

    if (!key.packageId.empty() || !key.namespaceId.empty())
    {
        appendCandidate(candidates,
                        LookupCandidate{key.packageId.empty() ? scope.packageId : key.packageId,
                                        key.namespaceId.empty() ? scope.namespaceId : key.namespaceId,
                                        key.key});
        return candidates;
    }

    appendKnownNamespaceCandidates(key.key, candidates);

    if (!scope.packageId.empty() || !scope.namespaceId.empty())
        appendCandidate(candidates, LookupCandidate{scope.packageId, scope.namespaceId, key.key});

    appendCandidate(candidates, LookupCandidate{{}, {}, key.key});
    return candidates;
}

std::vector<UiLocaleId>
UiLocalizationRuntime::fallbackLocales(const std::vector<LookupCandidate>& candidates) const
{
    std::vector<UiLocaleId> result;
    appendLocaleWithParents(result, activeLocale);

    const std::vector<UiLocaleId> activeChain = uiLocaleFallbackChain(activeLocale);
    for (const auto& [_, record] : catalogs)
    {
        const UiLocalizationAsset& asset = record.asset;
        if (!asset.fallbackLocale)
            continue;
        if (std::find(activeChain.begin(), activeChain.end(), asset.locale) == activeChain.end())
            continue;
        if (std::none_of(candidates.begin(),
                         candidates.end(),
                         [&asset, this](const LookupCandidate& candidate)
                         {
                             return catalogMatchesCandidate(asset, candidate);
                         }))
            continue;
        appendLocaleWithParents(result, *asset.fallbackLocale);
    }

    appendLocaleWithParents(result, fallbackLocale);

    for (const auto& [_, record] : catalogs)
    {
        const UiLocalizationAsset& asset = record.asset;
        if (!asset.sourceLocale)
            continue;
        if (std::none_of(candidates.begin(),
                         candidates.end(),
                         [&asset, this](const LookupCandidate& candidate)
                         {
                             return catalogMatchesCandidate(asset, candidate);
                         }))
            continue;
        appendLocaleWithParents(result, *asset.sourceLocale);
    }
    return result;
}

const UiLocalizationRuntime::CatalogRecord*
UiLocalizationRuntime::findEntry(const LookupCandidate& candidate,
                                 const UiLocaleId& locale,
                                 UiLocalizationEntry& outEntry) const
{
    const CatalogRecord* best = nullptr;
    const UiLocalizationEntry* bestEntry = nullptr;
    for (const auto& [_, record] : catalogs)
    {
        const UiLocalizationAsset& asset = record.asset;
        if (asset.locale != locale || !catalogMatchesCandidate(asset, candidate))
            continue;

        const auto entryIt = asset.entries.find(candidate.key);
        if (entryIt == asset.entries.end())
            continue;

        if (best == nullptr || record.order > best->order)
        {
            best = &record;
            bestEntry = &entryIt->second;
        }
    }

    if (bestEntry != nullptr)
        outEntry = *bestEntry;
    return best;
}

bool UiLocalizationRuntime::catalogMatchesCandidate(const UiLocalizationAsset& asset,
                                                    const LookupCandidate& candidate) const
{
    if (!candidate.packageId.empty() && asset.packageId != candidate.packageId)
        return false;
    if (!candidate.namespaceId.empty() && asset.namespaceId != candidate.namespaceId)
        return false;
    return true;
}

void UiLocalizationRuntime::appendKnownNamespaceCandidates(
    const std::string& key,
    std::vector<LookupCandidate>& candidates) const
{
    struct KnownNamespace
    {
        std::string packageId;
        std::string namespaceId;
    };

    std::vector<KnownNamespace> namespaces;
    for (const auto& [_, record] : catalogs)
    {
        const KnownNamespace known{record.asset.packageId, record.asset.namespaceId};
        const auto it = std::find_if(namespaces.begin(),
                                     namespaces.end(),
                                     [&known](const KnownNamespace& existing)
                                     {
                                         return existing.packageId == known.packageId &&
                                                existing.namespaceId == known.namespaceId;
                                     });
        if (it == namespaces.end())
            namespaces.push_back(known);
    }

    std::sort(namespaces.begin(),
              namespaces.end(),
              [](const KnownNamespace& lhs, const KnownNamespace& rhs)
              {
                  if (lhs.namespaceId.size() != rhs.namespaceId.size())
                      return lhs.namespaceId.size() > rhs.namespaceId.size();
                  return lhs.namespaceId < rhs.namespaceId;
              });

    for (const KnownNamespace& known : namespaces)
    {
        const std::string prefix = known.namespaceId + ".";
        if (!known.namespaceId.empty() && key.size() > prefix.size() &&
            key.substr(0, prefix.size()) == prefix)
        {
            appendCandidate(candidates,
                            LookupCandidate{known.packageId,
                                            known.namespaceId,
                                            key.substr(prefix.size())});
        }
    }
}

void UiLocalizationRuntime::addDiagnostic(const UiLocalizationKey& key,
                                          const UiLocalizationScope& scope,
                                          std::string message) const
{
    UiLocalizationDiagnostic diagnostic;
    diagnostic.key = key;
    diagnostic.scope = scope;
    diagnostic.locale = activeLocale;
    diagnostic.message = std::move(message);
    lastDiagnostics.push_back(std::move(diagnostic));
}
