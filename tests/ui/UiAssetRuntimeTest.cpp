#include <algorithm>
#include <cstdlib>
#include <filesystem>
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
            std::cerr << "UI asset runtime test failed: " << message << std::endl;
            std::exit(1);
        }
    }

    UiTheme testTheme(const std::string& extraStyle = {})
    {
        UiTheme theme = defaultUiTheme();
        theme.setStyleClass("Button.Hot", UiStyleClass{});
        theme.setStyleClass("Text.Body", UiStyleClass{});
        if (!extraStyle.empty())
            theme.setStyleClass(extraStyle, UiStyleClass{});
        return theme;
    }

    const char* baseButtonAssetJson(const char* label)
    {
        return label[0] == 'R'
            ? R"json({
  "schema": 1,
  "id": "ui.hot.base",
  "version": 2,
  "displayName": "Hot Reload Base Button",
  "dependencies": ["theme:ui.theme"],
  "parameters": [
    {"name": "label", "type": "String", "default": "Reloaded"},
    {"name": "style", "type": "String", "default": "Button.Hot"}
  ],
  "root": {
    "id": "button",
    "type": "Button",
    "text": "{{label}}",
    "styleClass": "{{style}}",
    "semantics": {"role": "Button", "name": "{{label}}"}
  }
})json"
            : R"json({
  "schema": 1,
  "id": "ui.hot.base",
  "version": 1,
  "displayName": "Hot Reload Base Button",
  "dependencies": ["theme:ui.theme"],
  "parameters": [
    {"name": "label", "type": "String", "default": "Original"},
    {"name": "style", "type": "String", "default": "Button.Hot"}
  ],
  "root": {
    "id": "button",
    "type": "Button",
    "text": "{{label}}",
    "styleClass": "{{style}}",
    "semantics": {"role": "Button", "name": "{{label}}"}
  }
})json";
    }

    const char* derivedButtonAssetJson()
    {
        return R"json({
  "schema": 1,
  "id": "ui.hot.derived",
  "version": 1,
  "base": "ui.hot.base",
  "displayName": "Derived Hot Button"
})json";
    }

    const char* invalidBaseButtonAssetJson()
    {
        return R"json({
  "schema": 1,
  "id": "ui.hot.base",
  "version": 3,
  "base": "ui.missing.base"
})json";
    }

    UiWidgetAssetDefinition parseAsset(const char* json)
    {
        UiWidgetAssetDefinition asset;
        UiSerializedValidationResult validation;
        require(parseUiWidgetAssetDefinition(json, asset, &validation), "widget asset did not parse");
        require(validation.valid(), "widget asset parse reported validation errors");
        return asset;
    }

    const UiTextData& textPayload(UiSystem& ui, UiHandle handle)
    {
        const UiNode* node = ui.findNode(handle);
        require(node != nullptr && node->type == UiNodeType::Text, "text node missing");
        return std::get<UiTextData>(node->payload);
    }

    void registerHotAssets(UiSystem& ui)
    {
        UiSerializedValidationResult validation;
        require(ui.uiAssets().registerThemeAsset(ui, "ui.theme", testTheme()),
                "theme asset did not register");
        require(ui.uiAssets().registerWidgetAsset(ui, parseAsset(baseButtonAssetJson("Original")), &validation),
                "base widget asset did not register");
        require(validation.valid(), "base widget validation failed");
        require(ui.uiAssets().registerWidgetAsset(ui, parseAsset(derivedButtonAssetJson()), &validation),
                "derived widget asset did not register");
        require(validation.valid(), "derived widget validation failed");
    }

    void testDependencyGraphAndReloadPreservesFocus()
    {
        UiSystem ui(nullptr);
        ui.setTheme(testTheme());
        registerHotAssets(ui);

        const std::vector<UiAssetReference> dependents =
            ui.uiAssets().dependentsOf(UiAssetReference{UiAssetType::WidgetAsset, "ui.hot.base"}, true);
        require(std::find(dependents.begin(),
                          dependents.end(),
                          UiAssetReference{UiAssetType::WidgetAsset, "ui.hot.derived"}) != dependents.end(),
                "derived widget asset was not a dependent of its base");

        const UiMountId mount = ui.createMount();
        UiWidgetAssetInstanceDesc desc;
        desc.assetId = "ui.hot.derived";
        UiSerializedLoadResult load = ui.instantiateWidgetAsset(desc, mount);
        require(load.success, "derived asset did not instantiate");
        require(textPayload(ui, load.instance.handles.at("button.label")).text == "Original",
                "initial text did not instantiate");

        UiSurface* surface = ui.findSurface(ui.getDefaultSurface());
        require(surface != nullptr, "default surface missing");
        require(surface->focusManager().requestFocus(surface->document(),
                                                     load.instance.handles.at("button"),
                                                     UiFocusReason::Programmatic),
                "initial focus request failed");

        const std::string consumerId = uiWidgetAssetConsumerId(desc, ui.getDefaultSurface(), mount);
        require(ui.uiAssets().queueWidgetAssetReload(parseAsset(baseButtonAssetJson("Reloaded"))),
                "base reload was not queued");
        UiAssetReloadResult result = ui.processUiAssetReloads();
        require(result.success, "valid base reload failed");
        require(result.rebuiltConsumers == 1, "base reload should rebuild one mounted consumer");

        const UiAssetConsumerState* consumer = ui.uiAssets().findConsumer(consumerId);
        require(consumer != nullptr, "live consumer missing after reload");
        require(consumer->desc.mount != mount, "consumer mount was not recreated");
        require(ui.findNode(ui.getDefaultSurface(), load.instance.handles.at("button")) == nullptr,
                "old mounted subtree survived reload");
        require(textPayload(ui, consumer->loadResult.instance.handles.at("button.label")).text == "Reloaded",
                "reloaded text did not reach runtime");
        require(surface->focusManager().focusedNode() == consumer->loadResult.instance.handles.at("button"),
                "focus was not restored to the reloaded widget id");
    }

    void testFailedReloadKeepsLastValidRuntime()
    {
        UiSystem ui(nullptr);
        ui.setTheme(testTheme());
        registerHotAssets(ui);

        const UiMountId mount = ui.createMount();
        UiWidgetAssetInstanceDesc desc;
        desc.assetId = "ui.hot.derived";
        UiSerializedLoadResult load = ui.instantiateWidgetAsset(desc, mount);
        require(load.success, "derived asset did not instantiate for failed reload test");
        const std::string consumerId = uiWidgetAssetConsumerId(desc, ui.getDefaultSurface(), mount);

        require(ui.uiAssets().queueWidgetAssetReload(parseAsset(invalidBaseButtonAssetJson())),
                "invalid reload was not queued");
        UiAssetReloadResult result = ui.processUiAssetReloads();
        require(!result.success, "invalid reload reported success");
        require(result.rebuiltConsumers == 0, "invalid reload rebuilt a consumer");

        const UiAssetConsumerState* consumer = ui.uiAssets().findConsumer(consumerId);
        require(consumer != nullptr && consumer->desc.mount == mount,
                "invalid reload should keep the existing consumer mount");
        require(textPayload(ui, consumer->loadResult.instance.handles.at("button.label")).text == "Original",
                "invalid reload replaced the last valid runtime");
        const UiWidgetAssetDefinition* base = ui.widgetAssets().findAsset("ui.hot.base");
        require(base != nullptr && base->version == 1,
                "invalid reload replaced the widget registry version");
    }

    void testThemeReloadAndSourceReload()
    {
        UiSystem ui(nullptr);
        registerHotAssets(ui);

        UiAssetConsumerDesc themeConsumer;
        themeConsumer.id = "default-surface-theme";
        themeConsumer.kind = UiAssetConsumerKind::SurfaceTheme;
        themeConsumer.surface = ui.getDefaultSurface();
        themeConsumer.themeAssetId = "ui.theme";
        require(ui.uiAssets().trackConsumer(themeConsumer), "surface theme consumer did not register");

        UiTheme reloadedTheme = testTheme("Theme.Reloaded");
        require(ui.uiAssets().queueThemeReload("ui.theme", reloadedTheme),
                "theme reload was not queued");
        UiAssetReloadResult themeResult = ui.processUiAssetReloads();
        require(themeResult.success, "theme reload failed");
        require(ui.theme().findStyleClass("Theme.Reloaded") != nullptr,
                "surface theme consumer did not apply reloaded theme");

        UiWidgetAssetDefinition base = parseAsset(baseButtonAssetJson("Original"));
        const std::filesystem::path path =
            std::filesystem::temp_directory_path() / "gravitas_ui_asset_runtime_reload.uiwidget.json";
        std::string error;
        require(saveUiWidgetAssetToFile(path.string(), base, &error), "initial source asset save failed: " + error);

        UiSystem sourceUi(nullptr);
        sourceUi.setTheme(testTheme());
        require(sourceUi.uiAssets().registerWidgetAsset(sourceUi, base, nullptr, path),
                "source-backed widget asset did not register");
        UiWidgetAssetDefinition reloaded = parseAsset(baseButtonAssetJson("Reloaded"));
        require(saveUiWidgetAssetToFile(path.string(), reloaded, &error), "updated source asset save failed: " + error);
        require(sourceUi.uiAssets().queueReloadFromSource(UiAssetReference{UiAssetType::WidgetAsset, "ui.hot.base"}),
                "source reload was not queued");
        UiAssetReloadResult sourceResult = sourceUi.processUiAssetReloads();
        require(sourceResult.success, "source reload failed");
        const UiAssetRecord* record = sourceUi.uiAssets().findAsset(UiAssetReference{UiAssetType::WidgetAsset, "ui.hot.base"});
        require(record != nullptr && record->widgetAsset && record->widgetAsset->version == 2,
                "source reload did not update the asset record");
    }
}

int main()
{
    testDependencyGraphAndReloadPreservesFocus();
    testFailedReloadKeepsLastValidRuntime();
    testThemeReloadAndSourceReload();
    return 0;
}
