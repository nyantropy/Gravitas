#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

#include "UiSystem.h"

namespace
{
    void require(bool condition, const std::string& message)
    {
        if (!condition)
        {
            std::cerr << "UI localization runtime test failed: " << message << std::endl;
            std::exit(1);
        }
    }

    UiLocalizationAsset catalog(const std::string& id,
                                const std::string& packageId,
                                const std::string& namespaceId,
                                const std::string& locale,
                                std::initializer_list<std::pair<const std::string, UiLocalizationEntry>> entries)
    {
        UiLocalizationAsset asset;
        asset.asset = UiAssetReference{UiAssetType::Localization, id};
        asset.packageId = packageId;
        asset.namespaceId = namespaceId;
        asset.locale = parseUiLocaleId(locale);
        asset.sourceLocale = parseUiLocaleId("en-US");
        for (const auto& [key, entry] : entries)
            asset.entries[key] = entry;
        return asset;
    }

    UiLocalizationAsset gameEnUsCatalog()
    {
        UiLocalizationAsset asset = catalog("game.ui.locale.en-US",
                                            "game.ui",
                                            "game.ui",
                                            "en-US",
                                            {{"interaction_prompt.label",
                                              UiLocalizationEntry{
                                                  .text = "Press {input} to interact",
                                                  .context = "interaction prompt label",
                                                  .translatorNote = "Shown near the bottom of the screen"}},
                                             {"menu.start", UiLocalizationEntry{.text = "Start"}}});
        asset.fallbackLocale = parseUiLocaleId("en");
        return asset;
    }

    UiLocalizationAsset gameEnCatalog()
    {
        return catalog("game.ui.locale.en",
                       "game.ui",
                       "game.ui",
                       "en",
                       {{"interaction_prompt.label", UiLocalizationEntry{.text = "Interact"}},
                        {"menu.start", UiLocalizationEntry{.text = "Start Default"}},
                        {"parent.only", UiLocalizationEntry{.text = "Parent English"}}});
    }

    UiLocalizationAsset gameDeCatalog()
    {
        return catalog("game.ui.locale.de",
                       "game.ui",
                       "game.ui",
                       "de",
                       {{"menu.start", UiLocalizationEntry{.text = "Starten"}}});
    }

    UiPackageDesc enginePackage()
    {
        UiPackageDesc package;
        package.manifest.id = "engine.ui";
        package.manifest.displayName = "Engine UI";
        package.manifest.author = "Gravitas";
        package.manifest.version = "1.0.0";
        package.manifest.namespaceId = "engine.ui";

        UiPackageAssetDesc localization;
        localization.reference = UiAssetReference{UiAssetType::Localization, "engine.ui.locale.en-US"};
        localization.localizationAsset = catalog("engine.ui.locale.en-US",
                                                 "engine.ui",
                                                 "engine.ui",
                                                 "en-US",
                                                 {{"ok", UiLocalizationEntry{.text = "OK"}}});
        package.assets.push_back(std::move(localization));
        return package;
    }

    UiPackageDesc gamePackage()
    {
        UiPackageDesc package;
        package.manifest.id = "game.ui";
        package.manifest.displayName = "Game UI";
        package.manifest.author = "Game";
        package.manifest.version = "1.0.0";
        package.manifest.namespaceId = "game.ui";
        package.manifest.dependencies.push_back(UiPackageDependency{.packageId = "engine.ui", .minVersion = "1.0.0"});

        UiPackageAssetDesc localization;
        localization.reference = UiAssetReference{UiAssetType::Localization, "game.ui.locale.en-US"};
        localization.localizationAsset = gameEnUsCatalog();
        package.assets.push_back(std::move(localization));
        return package;
    }

    void testLocaleParsingAndParentChain()
    {
        require(parseUiLocaleId("en_us").tag == "en-US", "locale underscore normalization failed");
        require(parseUiLocaleId("ZH_hant_tw").tag == "zh-Hant-TW", "locale script/region normalization failed");

        const std::vector<UiLocaleId> chain = uiLocaleFallbackChain(parseUiLocaleId("de-AT"));
        require(chain.size() == 2 && chain[0].tag == "de-AT" && chain[1].tag == "de",
                "parent locale fallback chain mismatch");
    }

    void testCatalogParseValidateAndRoundTrip()
    {
        UiLocalizationAsset asset;
        UiSerializedValidationResult validation;
        require(parseUiLocalizationAsset(R"json({
  "schema": 1,
  "id": "game.ui.locale.en-US",
  "package": "game.ui",
  "namespace": "game.ui",
  "locale": "en-US",
  "sourceLocale": "en-US",
  "fallbackLocale": "en",
  "entries": {
    "interaction_prompt.label": {
      "text": "Press {input} to interact",
      "context": "interaction prompt label",
      "note": "Shown near the bottom of the screen"
    },
    "menu.start": {
      "text": "Start"
    }
  }
})json",
                                           asset,
                                           &validation),
                "localization asset did not parse");
        require(validation.valid(), "valid localization asset reported validation errors");
        require(asset.asset.type == UiAssetType::Localization, "localization asset reference type mismatch");
        require(asset.locale.tag == "en-US", "locale did not parse");
        require(asset.fallbackLocale && asset.fallbackLocale->tag == "en", "fallback locale did not parse");
        require(asset.entries.at("interaction_prompt.label").translatorNote == "Shown near the bottom of the screen",
                "translator note did not parse");

        const std::string serialized = serializeUiLocalizationAsset(asset);
        UiLocalizationAsset reparsed;
        require(parseUiLocalizationAsset(serialized, reparsed), "serialized localization asset did not parse");
        require(reparsed.entries.at("menu.start").text == "Start", "localization asset did not round-trip");

        UiLocalizationAsset invalid = asset;
        invalid.schemaVersion = 99;
        UiLocalizationRuntime runtime;
        UiSerializedValidationResult invalidResult = runtime.validateCatalog(invalid);
        require(!invalidResult.valid(), "unsupported localization schema was accepted");

        invalid = asset;
        invalid.entries["bad"] = UiLocalizationEntry{.text = "Broken {placeholder"};
        invalidResult = runtime.validateCatalog(invalid);
        require(!invalidResult.valid(), "unbalanced placeholder braces were accepted");
    }

    void testRegistrationLookupAndFallbacks()
    {
        UiLocalizationRuntime runtime;
        UiSerializedValidationResult validation;
        require(runtime.registerCatalog(gameEnUsCatalog(), &validation), "en-US catalog registration failed");
        require(validation.valid(), "en-US catalog validation failed");
        require(runtime.registerCatalog(gameEnCatalog()), "en catalog registration failed");
        require(runtime.registerCatalog(gameDeCatalog()), "de catalog registration failed");

        runtime.setLocale("en-US");
        UiLocalizationScope scope{.packageId = "game.ui", .namespaceId = "game.ui"};

        UiLocalizedString relative = runtime.resolveKey("interaction_prompt.label", scope);
        require(relative.found && relative.text == "Press {input} to interact",
                "package-relative localization lookup failed");
        require(!relative.usedFallbackLocale, "primary locale incorrectly reported fallback");

        UiLocalizedString absolute = runtime.resolveKey("game.ui.interaction_prompt.label");
        require(absolute.found && absolute.text == "Press {input} to interact",
                "absolute namespace localization lookup failed");

        runtime.setLocale("de-AT");
        UiLocalizedString parent = runtime.resolveKey("menu.start", scope);
        require(parent.found && parent.text == "Starten" && parent.locale.tag == "de",
                "parent locale fallback failed");

        runtime.setLocale("fr-FR");
        runtime.setDefaultLocale("en");
        UiLocalizedString defaulted = runtime.resolveKey("menu.start", scope);
        require(defaulted.found && defaulted.text == "Start Default" && defaulted.usedFallbackLocale,
                "default locale fallback failed");

        UiLocalizedString inlineFallback = runtime.resolveKey("missing.key", scope, "Inline fallback");
        require(inlineFallback.text == "Inline fallback" && inlineFallback.usedInlineFallback && inlineFallback.missing,
                "inline fallback was not used for missing key");
        require(!runtime.diagnostics().empty(), "missing key diagnostic was not recorded");
        require(runtime.drainDiagnostics().size() >= 1, "diagnostic drain did not return missing key");
        require(runtime.diagnostics().empty(), "diagnostic drain did not clear diagnostics");

        require(runtime.unregisterCatalog(UiAssetReference{UiAssetType::Localization, "game.ui.locale.de"}),
                "catalog unregister failed");
        runtime.setLocale("de-AT");
        UiLocalizedString afterUnregister = runtime.resolveKey("menu.start", scope);
        require(afterUnregister.found && afterUnregister.text == "Start Default",
                "unregistered locale catalog still influenced lookup");
    }

    void testUiSystemFacadeAndPackageOwnedCatalogs()
    {
        UiSystem ui(nullptr);
        require(ui.setLocale("en-US"), "UiSystem locale setter rejected valid locale");
        require(ui.locale().tag == "en-US", "UiSystem locale getter mismatch");

        UiPackageLoadResult missing = ui.packages().registerPackage(ui, gamePackage());
        require(!missing.success, "game package loaded without required engine package");

        UiPackageLoadResult engine = ui.packages().registerPackage(ui, enginePackage());
        require(engine.success, "engine localization package failed");
        UiPackageLoadResult game = ui.packages().registerPackage(ui, gamePackage());
        require(game.success, "game localization package failed");

        UiLocalizationScope scope{.packageId = "game.ui", .namespaceId = "game.ui"};
        UiLocalizedString text = ui.localization().resolveKey("interaction_prompt.label", scope);
        require(text.found && text.text == "Press {input} to interact",
                "package-owned localization catalog did not resolve");

        UiLocalizedTextRef ref;
        ref.key.key = "interaction_prompt.label";
        text = ui.localize(ref, scope);
        require(text.found && text.text == "Press {input} to interact",
                "UiSystem localize facade failed");

        require(ui.localization().catalogAssets().size() == 2, "package catalogs were not registered");
        UiPackageLoadResult unload = ui.packages().unregisterPackage(ui, "game.ui");
        require(unload.success, "game localization package unload failed");
        text = ui.localization().resolveKey("interaction_prompt.label", scope, "Missing");
        require(!text.found && text.usedInlineFallback && text.text == "Missing",
                "package unload did not unregister localization catalog");
    }
}

int main()
{
    testLocaleParsingAndParentChain();
    testCatalogParseValidateAndRoundTrip();
    testRegistrationLookupAndFallbacks();
    testUiSystemFacadeAndPackageOwnedCatalogs();
    return 0;
}
