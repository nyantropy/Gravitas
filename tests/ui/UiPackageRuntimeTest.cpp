#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <string>
#include <variant>

#include "UiSystem.h"

namespace
{
    void require(bool condition, const std::string& message)
    {
        if (!condition)
        {
            std::cerr << "UI package runtime test failed: " << message << std::endl;
            std::exit(1);
        }
    }

    UiTheme packageTheme()
    {
        UiTheme theme = defaultUiTheme();
        theme.setStyleClass("Button.Package", UiStyleClass{});
        theme.setStyleClass("Panel.Package", UiStyleClass{});
        return theme;
    }

    const char* engineButtonJson(const char* label, int version)
    {
        return version == 2
            ? R"json({
  "schema": 1,
  "id": "engine.ui.button",
  "version": 2,
  "displayName": "Overridden Engine Button",
  "dependencies": ["theme:engine.ui.theme"],
  "parameters": [
    {"name": "label", "type": "String", "default": "Game Button"}
  ],
  "root": {
    "id": "button",
    "type": "Button",
    "text": "{{label}}",
    "styleClass": "Button.Package",
    "semantics": {"role": "Button", "name": "{{label}}"}
  }
})json"
            : R"json({
  "schema": 1,
  "id": "engine.ui.button",
  "version": 1,
  "displayName": "Engine Button",
  "dependencies": ["theme:engine.ui.theme"],
  "parameters": [
    {"name": "label", "type": "String", "default": "Engine Button"}
  ],
  "root": {
    "id": "button",
    "type": "Button",
    "text": "{{label}}",
    "styleClass": "Button.Package",
    "semantics": {"role": "Button", "name": "{{label}}"}
  }
})json";
    }

    const char* gamePanelJson()
    {
        return R"json({
  "schema": 1,
  "id": "game.ui.panel",
  "version": 1,
  "displayName": "Game Panel",
  "root": {
    "id": "panel",
    "type": "Panel",
    "styleClass": "Panel.Package"
  }
})json";
    }

    UiWidgetAssetDefinition parseWidgetAsset(const char* json)
    {
        UiWidgetAssetDefinition asset;
        UiSerializedValidationResult validation;
        require(parseUiWidgetAssetDefinition(json, asset, &validation), "widget asset did not parse");
        require(validation.valid(), "widget asset parse reported validation errors");
        return asset;
    }

    UiPackageDesc enginePackage()
    {
        UiPackageDesc package;
        package.manifest.id = "engine.ui";
        package.manifest.displayName = "Engine UI";
        package.manifest.author = "Gravitas";
        package.manifest.version = "1.0.0";
        package.manifest.namespaceId = "engine.ui";
        package.manifest.tags = {"engine", "ui"};

        UiPackageAssetDesc theme;
        theme.reference = UiAssetReference{UiAssetType::Theme, "engine.ui.theme"};
        theme.theme = packageTheme();
        package.assets.push_back(std::move(theme));

        UiPackageAssetDesc button;
        button.reference = UiAssetReference{UiAssetType::WidgetAsset, "engine.ui.button"};
        button.widgetAsset = parseWidgetAsset(engineButtonJson("Engine Button", 1));
        package.assets.push_back(std::move(button));
        return package;
    }

    UiPackageDesc gamePackage(bool overrideButton = true)
    {
        UiPackageDesc package;
        package.manifest.id = "game.ui";
        package.manifest.displayName = "Game UI";
        package.manifest.author = "Game";
        package.manifest.version = "1.0.0";
        package.manifest.namespaceId = "game.ui";
        package.manifest.dependencies.push_back(UiPackageDependency{.packageId = "engine.ui", .minVersion = "1.0.0"});
        package.manifest.plugins.push_back(UiPluginMetadata{
            .id = "game.ui.inspector",
            .displayName = "Game UI Inspector",
            .version = "1.0.0",
            .description = "Metadata-only test plugin",
            .capabilities = {"validator", "editor-panel"},
            .nativeCode = false});

        UiPackageAssetDesc panel;
        panel.reference = UiAssetReference{UiAssetType::WidgetAsset, "game.ui.panel"};
        panel.widgetAsset = parseWidgetAsset(gamePanelJson());
        package.assets.push_back(std::move(panel));

        UiPackageAssetDesc button;
        button.reference = UiAssetReference{UiAssetType::WidgetAsset, "engine.ui.button"};
        button.overridePolicy = overrideButton
            ? UiPackageAssetOverridePolicy::Replace
            : UiPackageAssetOverridePolicy::None;
        button.widgetAsset = parseWidgetAsset(engineButtonJson("Game Button", 2));
        package.assets.push_back(std::move(button));
        return package;
    }

    const UiTextData& textPayload(UiSystem& ui, UiHandle handle)
    {
        const UiNode* node = ui.findNode(handle);
        require(node != nullptr && node->type == UiNodeType::Text, "text node missing");
        return std::get<UiTextData>(node->payload);
    }

    void testManifestParseRoundTripAndPluginMetadata()
    {
        UiPackageManifest manifest;
        UiSerializedValidationResult validation;
        require(parseUiPackageManifest(R"json({
  "schema": 1,
  "id": "game.ui",
  "displayName": "Game UI",
  "author": "Game Team",
  "version": "2.3.4",
  "namespace": "game.ui",
  "engineCompatibility": ">=1.0.0",
  "description": "Package manifest test.",
  "tags": ["game", "ui"],
  "assetRoots": ["resources/ui"],
  "dependencies": [{"id": "engine.ui", "minVersion": "1.0.0"}],
  "optionalDependencies": [{"id": "mod.dark", "minVersion": "1.2.0", "optional": true}],
  "plugins": [{
    "id": "game.ui.inspector",
    "displayName": "Inspector",
    "version": "1.0.0",
    "capabilities": ["validator"],
    "nativeCode": false
  }]
})json",
                                       manifest,
                                       &validation),
                "package manifest did not parse");
        require(manifest.id == "game.ui", "manifest id mismatch");
        require(manifest.dependencies.size() == 1, "manifest dependencies missing");
        require(manifest.optionalDependencies.size() == 1 && manifest.optionalDependencies[0].optional,
                "manifest optional dependency missing");
        require(manifest.plugins.size() == 1 && manifest.plugins[0].capabilities.size() == 1,
                "plugin metadata missing");

        const std::string serialized = serializeUiPackageManifest(manifest);
        UiPackageManifest reparsed;
        require(parseUiPackageManifest(serialized, reparsed), "serialized manifest did not parse");
        require(reparsed.namespaceId == "game.ui" && reparsed.plugins.size() == 1,
                "manifest did not round-trip");
        require(compareUiPackageVersions(parseUiPackageVersion("2.3.4"),
                                         parseUiPackageVersion("2.2.9")) > 0,
                "semantic version comparison failed");
    }

    void testDependencyValidationLoadOrderAndCycles()
    {
        UiSystem ui(nullptr);
        UiPackageLoadResult missing = ui.packages().registerPackage(ui, gamePackage());
        require(!missing.success, "package with missing required dependency loaded");

        UiPackageLoadResult engine = ui.packages().registerPackage(ui, enginePackage());
        require(engine.success, "engine package did not load");

        UiPackageDesc tooNew = gamePackage();
        tooNew.manifest.dependencies[0].minVersion = "9.0.0";
        UiPackageLoadResult versionMismatch = ui.packages().registerPackage(ui, tooNew);
        require(!versionMismatch.success, "version-mismatched package loaded");

        UiPackageLoadResult game = ui.packages().registerPackage(ui, gamePackage());
        require(game.success, "game package did not load after engine dependency");
        std::vector<std::string> order = ui.packages().packageLoadOrder();
        require(order.size() == 2 && order[0] == "engine.ui" && order[1] == "game.ui",
                "package load order did not keep dependency before dependent");

        UiPackageDesc cyclicEngine = enginePackage();
        cyclicEngine.manifest.dependencies.push_back(UiPackageDependency{.packageId = "game.ui"});
        UiPackageLoadResult cycle = ui.packages().registerPackage(ui, cyclicEngine);
        require(!cycle.success, "cyclic package dependency was accepted");
    }

    void testOverrideNamespaceLookupHotReloadAndUnloadFallback()
    {
        UiSystem ui(nullptr);
        ui.setTheme(packageTheme());
        require(ui.packages().registerPackage(ui, enginePackage()).success, "engine package failed");

        UiWidgetAssetInstanceDesc desc;
        desc.assetId = "engine.ui.button";
        const UiMountId mount = ui.createMount();
        UiSerializedLoadResult load = ui.instantiateWidgetAsset(desc, mount);
        require(load.success, "engine button did not instantiate");
        require(textPayload(ui, load.instance.handles.at("button.label")).text == "Engine Button",
                "engine package asset text mismatch");

        const std::string consumerId = uiWidgetAssetConsumerId(desc, ui.getDefaultSurface(), mount);
        UiPackageLoadResult game = ui.packages().registerPackage(ui, gamePackage());
        require(game.success, "game override package failed");
        require(game.assetReload.rebuiltConsumers == 1, "override package did not rebuild mounted consumer");
        const UiPackageAssetOrigin* origin =
            ui.packages().findAssetOrigin(UiAssetReference{UiAssetType::WidgetAsset, "engine.ui.button"});
        require(origin != nullptr && origin->packageId == "game.ui", "override origin not recorded");

        const UiAssetConsumerState* consumer = ui.uiAssets().findConsumer(consumerId);
        require(consumer != nullptr, "consumer missing after package override");
        require(textPayload(ui, consumer->loadResult.instance.handles.at("button.label")).text == "Game Button",
                "override package asset did not reach runtime");
        require(ui.packages().resolveAsset(UiAssetType::WidgetAsset, "panel", "game.ui").id == "game.ui.panel",
                "requesting package namespace lookup failed");

        UiPackageLoadResult unload = ui.packages().unregisterPackage(ui, "game.ui");
        require(unload.success, "game package unload failed");
        require(unload.assetReload.rebuiltConsumers == 1, "unload fallback did not rebuild mounted consumer");
        origin = ui.packages().findAssetOrigin(UiAssetReference{UiAssetType::WidgetAsset, "engine.ui.button"});
        require(origin != nullptr && origin->packageId == "engine.ui", "unload did not restore engine asset origin");

        consumer = ui.uiAssets().findConsumer(consumerId);
        require(consumer != nullptr, "consumer missing after package unload");
        require(textPayload(ui, consumer->loadResult.instance.handles.at("button.label")).text == "Engine Button",
                "package unload did not restore fallback runtime asset");
    }

    void testDuplicateAssetRequiresOverridePolicy()
    {
        UiSystem ui(nullptr);
        require(ui.packages().registerPackage(ui, enginePackage()).success, "engine package failed");
        UiPackageLoadResult duplicate = ui.packages().registerPackage(ui, gamePackage(false));
        require(!duplicate.success, "duplicate asset without override policy loaded");
    }
}

int main()
{
    testManifestParseRoundTripAndPluginMetadata();
    testDependencyValidationLoadOrderAndCycles();
    testOverrideNamespaceLookupHotReloadAndUnloadFallback();
    testDuplicateAssetRequiresOverridePolicy();
    return 0;
}
