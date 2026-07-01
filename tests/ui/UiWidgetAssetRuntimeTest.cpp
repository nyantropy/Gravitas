#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>

#include "UiSystem.h"
#include "UiWidgetAsset.h"

namespace
{
    void require(bool condition, const std::string& message)
    {
        if (!condition)
        {
            std::cerr << "UI widget asset runtime test failed: " << message << std::endl;
            std::exit(1);
        }
    }

    const char* baseButtonAssetJson()
    {
        return R"json({
  "schema": 1,
  "id": "ui.button.base",
  "version": 1,
  "displayName": "Base Button",
  "description": "Reusable themed button shell.",
  "tags": ["button", "control"],
  "parameters": [
    {"name": "label", "type": "String", "default": "Button"},
    {"name": "style", "type": "String", "default": "Button"},
    {"name": "labelStyle", "type": "String", "default": "Text.Body"},
    {"name": "navGroup", "type": "String", "default": "main"}
  ],
  "variants": [
    {
      "name": "primary",
      "displayName": "Primary",
      "parameters": {"style": "Button.Primary"}
    },
    {
      "name": "compact",
      "displayName": "Compact",
      "root": {
        "id": "button",
        "layout": {
          "constraints": {
            "preferredWidth": {"unit": "Normalized", "value": 0.22},
            "preferredHeight": {"unit": "Normalized", "value": 0.07}
          }
        }
      }
    }
  ],
  "root": {
    "id": "button",
    "type": "Button",
    "text": "{{label}}",
    "styleClass": "{{style}}",
    "labelStyleClass": "{{labelStyle}}",
    "horizontalAlign": "Center",
    "verticalAlign": "Middle",
    "layout": {
      "constraints": {
        "preferredWidth": {"unit": "Normalized", "value": 0.32},
        "preferredHeight": {"unit": "Normalized", "value": 0.09}
      }
    },
    "navigation": {
      "enabled": true,
      "role": "Button",
      "group": "{{navGroup}}",
      "tabIndex": 1
    },
    "semantics": {
      "role": "Button",
      "name": "{{label}}"
    }
  }
})json";
    }

    const char* dangerButtonAssetJson()
    {
        return R"json({
  "schema": 1,
  "id": "ui.button.danger",
  "version": 1,
  "base": "ui.button.base",
  "displayName": "Danger Button",
  "tags": ["danger"],
  "parameters": [
    {"name": "style", "type": "String", "default": "Button.Danger"}
  ]
})json";
    }

    const char* promptAssetJson()
    {
        return R"json({
  "schema": 1,
  "id": "ui.prompt.panel",
  "version": 1,
  "displayName": "Prompt Panel",
  "parameters": [
    {"name": "title", "type": "String", "default": "Prompt"},
    {"name": "message", "type": "String", "default": "Ready"}
  ],
  "slots": [
    {
      "name": "body",
      "target": "panel",
      "children": [
        {
          "id": "message",
          "type": "Label",
          "text": "{{message}}",
          "styleClass": "Text.Body"
        }
      ]
    }
  ],
  "root": {
    "id": "panel",
    "type": "Panel",
    "styleClass": "Panel",
    "semantics": {
      "role": "Panel",
      "name": "{{title}}"
    }
  }
})json";
    }

    UiTheme testTheme()
    {
        UiTheme theme = defaultUiTheme();
        theme.setStyleClass("Text.Body", UiStyleClass{});
        theme.setStyleClass("Button", UiStyleClass{});
        theme.setStyleClass("Button.Primary", UiStyleClass{});
        theme.setStyleClass("Button.Danger", UiStyleClass{});
        theme.setStyleClass("Panel", UiStyleClass{});
        return theme;
    }

    const UiTextData& textPayload(UiSystem& ui, UiHandle handle)
    {
        const UiNode* node = ui.findNode(handle);
        require(node != nullptr && node->type == UiNodeType::Text, "text node missing");
        return std::get<UiTextData>(node->payload);
    }

    UiWidgetAssetDefinition parseAsset(const char* json)
    {
        UiWidgetAssetDefinition asset;
        UiSerializedValidationResult validation;
        require(parseUiWidgetAssetDefinition(json, asset, &validation), "widget asset did not parse");
        require(validation.valid(), "widget asset parse reported errors");
        return asset;
    }

    void registerAssets(UiWidgetAssetRegistry& registry)
    {
        UiSerializedValidationResult validation;
        require(registry.registerAsset(parseAsset(baseButtonAssetJson()), &validation),
                "base button asset did not register");
        require(registry.registerAsset(parseAsset(dangerButtonAssetJson()), &validation),
                "danger button asset did not register");
        require(registry.registerAsset(parseAsset(promptAssetJson()), &validation),
                "prompt asset did not register");
    }

    void testParseRoundTripAndRegistry()
    {
        UiWidgetAssetDefinition asset = parseAsset(baseButtonAssetJson());
        const std::string serialized = serializeUiWidgetAssetDefinition(asset);
        UiWidgetAssetDefinition reparsed;
        require(parseUiWidgetAssetDefinition(serialized, reparsed), "round-trip widget asset did not parse");
        require(reparsed.id == "ui.button.base", "round-trip asset id mismatch");
        require(reparsed.variants.count("primary") == 1, "variant did not round-trip");

        UiWidgetAssetRegistry registry;
        registerAssets(registry);
        std::vector<std::string> ids = registry.assetIds();
        require(ids.size() == 3 && ids[0] == "ui.button.base", "registry ids not sorted or missing");
        require(registry.findAsset("ui.button.danger") != nullptr, "registered asset lookup failed");
    }

    void testInheritanceVariantsAndExpansion()
    {
        UiWidgetAssetRegistry registry;
        registerAssets(registry);

        UiWidgetAssetInstanceDesc primary;
        primary.assetId = "ui.button.base";
        primary.variant = "primary";
        primary.idPrefix = "apply";
        primary.parameterOverrides["label"] = "Apply";
        UiWidgetAssetExpandResult expandedPrimary = registry.expandAsset(primary);
        require(expandedPrimary.success, "primary button asset did not expand");
        require(expandedPrimary.widget.id == "apply", "asset id prefix did not replace root id");
        require(expandedPrimary.widget.styleClass == "Button.Primary", "variant parameter default did not apply");
        require(expandedPrimary.widget.text == "Apply", "parameter substitution did not apply");

        UiWidgetAssetInstanceDesc danger;
        danger.assetId = "ui.button.danger";
        danger.idPrefix = "delete";
        danger.parameterOverrides["label"] = "Delete";
        UiWidgetAssetExpandResult expandedDanger = registry.expandAsset(danger);
        require(expandedDanger.success, "derived danger button did not expand");
        require(expandedDanger.widget.styleClass == "Button.Danger", "derived default did not override base");
    }

    void testSlotsAndNestedAssetReferences()
    {
        UiWidgetAssetRegistry registry;
        registerAssets(registry);

        UiWidgetAssetInstanceDesc prompt;
        prompt.assetId = "ui.prompt.panel";
        prompt.idPrefix = "prompt";
        prompt.parameterOverrides["title"] = "Confirm";

        UiSerializedWidget okButton;
        okButton.id = "ok";
        okButton.asset = "ui.button.base";
        okButton.variant = "primary";
        okButton.parameters["label"] = "OK";
        prompt.slotContent["body"].push_back(okButton);

        UiWidgetAssetExpandResult expanded = registry.expandAsset(prompt);
        require(expanded.success, "prompt asset did not expand");
        require(expanded.widget.id == "prompt", "prompt root was not prefixed");
        require(expanded.widget.children.size() == 1, "slot override did not replace default slot content");
        require(expanded.widget.children[0].id == "prompt.ok", "slot child asset did not inherit prefix");
        require(expanded.widget.children[0].type == "Button", "nested asset reference was not expanded");
        require(expanded.widget.children[0].text == "OK", "nested asset parameter was not applied");
    }

    void testRuntimeInstantiationFromSerializedAssetReferences()
    {
        UiSystem ui(nullptr);
        ui.setTheme(testTheme());
        registerAssets(ui.widgetAssets());

        UiSerializedAsset document;
        require(parseUiSerializedAsset(R"json({
  "schema": 1,
  "id": "test.asset_document",
  "root": {
    "id": "root",
    "type": "Stack",
    "children": [
      {
        "id": "apply",
        "asset": "ui.button.base",
        "variant": "primary",
        "parameters": {"label": "Apply", "navGroup": "actions"}
      },
      {
        "id": "delete",
        "asset": "ui.button.danger",
        "parameters": {"label": "Delete"}
      }
    ]
  }
})json",
                                      document),
                "serialized document with asset references did not parse");

        const UiMountId mount = ui.createMount();
        UiSerializedLoadResult result = ui.instantiateUiAsset(document, mount);
        require(result.success, "serialized document with asset references did not instantiate");
        require(result.instance.handles.count("apply") == 1, "apply asset root handle missing");
        require(result.instance.handles.count("apply.label") == 1, "apply label attachment handle missing");
        require(result.instance.handles.count("delete") == 1, "delete asset root handle missing");

        const UiNode* apply = ui.findNode(result.instance.handles.at("apply"));
        require(apply != nullptr && apply->style.styleClass == "Button.Primary",
                "primary variant style class not applied at runtime");
        require(textPayload(ui, result.instance.handles.at("apply.label")).text == "Apply",
                "asset label text not instantiated");

        const UiNavigationNode* nav = ui.navigationGraph().findNode(result.instance.handles.at("apply"));
        require(nav != nullptr && nav->desc.group == "actions",
                "asset navigation metadata did not instantiate");
    }

    void testFileSaveAndLoad()
    {
        UiWidgetAssetDefinition asset = parseAsset(baseButtonAssetJson());
        const std::filesystem::path path =
            std::filesystem::temp_directory_path() / "gravitas_ui_widget_asset_test.uiwidget.json";
        std::string error;
        require(saveUiWidgetAssetToFile(path.string(), asset, &error),
                "widget asset save failed: " + error);

        UiWidgetAssetDefinition loaded;
        UiSerializedValidationResult validation;
        require(loadUiWidgetAssetFromFile(path.string(), loaded, &validation),
                "widget asset load failed");
        require(loaded.id == asset.id && loaded.parameters.count("label") == 1,
                "loaded widget asset did not match saved asset");
    }
}

int main()
{
    testParseRoundTripAndRegistry();
    testInheritanceVariantsAndExpansion();
    testSlotsAndNestedAssetReferences();
    testRuntimeInstantiationFromSerializedAssetReferences();
    testFileSaveAndLoad();
    return 0;
}
