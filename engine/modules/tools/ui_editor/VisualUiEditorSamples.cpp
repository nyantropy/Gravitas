#include "VisualUiEditorSamples.h"

#include <stdexcept>

#include "UiTheme.h"

namespace gts::tools
{
    namespace
    {
        UiWidgetAssetDefinition parseSample(const char* json)
        {
            UiWidgetAssetDefinition asset;
            UiSerializedValidationResult validation;
            if (!parseUiWidgetAssetDefinition(json, asset, &validation) || !validation.valid())
                throw std::runtime_error("Visual UI editor sample asset failed to parse");
            return asset;
        }

        UiLocalizationAsset engineLocalizationCatalog()
        {
            UiLocalizationAsset asset;
            asset.asset = UiAssetReference{UiAssetType::Localization, "engine.ui.locale.en-US"};
            asset.packageId = "engine.ui";
            asset.namespaceId = "engine";
            asset.locale = parseUiLocaleId("en-US");
            asset.sourceLocale = parseUiLocaleId("en-US");
            asset.entries["status_prompt.ready"] = UiLocalizationEntry{
                .text = "Ready",
                .context = "default status prompt message",
                .translatorNote = "Used by the Visual UI Editor sample prompt"};
            return asset;
        }

        UiLocalizationAsset gameLocalizationCatalog()
        {
            UiLocalizationAsset asset;
            asset.asset = UiAssetReference{UiAssetType::Localization, "game.ui.locale.en-US"};
            asset.packageId = "game.ui";
            asset.namespaceId = "dungeon";
            asset.locale = parseUiLocaleId("en-US");
            asset.sourceLocale = parseUiLocaleId("en-US");
            asset.fallbackLocale = parseUiLocaleId("en");
            asset.entries["interaction_prompt.label"] = UiLocalizationEntry{
                .text = "Press {input} to interact",
                .context = "interaction prompt label",
                .translatorNote = "Shown near the bottom of the screen"};
            return asset;
        }
    }

    void registerVisualUiEditorSampleThemeClasses(UiTheme& theme)
    {
        theme.setStyleClass("Panel.Prompt", UiStyleClass{});
        theme.setStyleClass("Panel.InteractionPrompt", UiStyleClass{});
        theme.setStyleClass("Text.Prompt", UiStyleClass{});
        theme.setStyleClass("Text.InteractionPrompt", UiStyleClass{});
        theme.setStyleClass("Text.EditorPreview", UiStyleClass{});
        theme.setStyleClass("Text.Body", UiStyleClass{});
    }

    UiWidgetAssetDefinition createVisualUiEditorStatusPromptAsset()
    {
        return parseSample(R"json({
  "schema": 1,
  "id": "engine.status_prompt",
  "version": 1,
  "displayName": "Status Prompt",
  "description": "Reusable prompt shell used by the Visual UI Editor sample workflow.",
  "tags": ["prompt", "sample", "ui-editor"],
  "parameters": [
    {"name": "message", "type": "String", "default": "Ready"},
    {"name": "style", "type": "String", "default": "Panel.Prompt"},
    {"name": "textStyle", "type": "String", "default": "Text.Prompt"}
  ],
  "root": {
    "id": "prompt",
    "type": "Panel",
    "styleClass": "{{style}}",
    "layout": {
      "type": "Stack",
      "direction": "Vertical",
      "padding": {"left": {"unit": "Normalized", "value": 0.020}, "right": {"unit": "Normalized", "value": 0.020}, "top": {"unit": "Normalized", "value": 0.010}, "bottom": {"unit": "Normalized", "value": 0.010}},
      "constraints": {
        "preferredWidth": {"unit": "Normalized", "value": 0.42},
        "preferredHeight": {"unit": "Normalized", "value": 0.11}
      }
    },
    "semantics": {"role": "Status", "name": "{{message}}", "nameKey": "status_prompt.ready"},
    "children": [
      {
        "id": "label",
        "type": "Label",
        "text": "{{message}}",
        "textKey": "status_prompt.ready",
        "styleClass": "{{textStyle}}",
        "horizontalAlign": "Center",
        "verticalAlign": "Middle",
        "layout": {
          "constraints": {
            "preferredHeight": {"unit": "Normalized", "value": 0.060}
          }
        },
        "semantics": {"role": "Label", "name": "{{message}}", "nameKey": "status_prompt.ready"}
      }
    ]
  }
})json");
    }

    UiWidgetAssetDefinition createVisualUiEditorInteractionPromptAsset()
    {
        return parseSample(R"json({
  "schema": 1,
  "id": "dungeon.interaction_prompt",
  "version": 1,
  "base": "engine.status_prompt",
  "displayName": "Interaction Prompt",
  "description": "Dungeon prompt asset used to demonstrate opening, editing, validating, previewing, and saving authored UI.",
  "tags": ["prompt", "dungeon", "interaction"],
  "parameters": [
    {"name": "message", "type": "String", "default": "Press E to interact"},
    {"name": "style", "type": "String", "default": "Panel.InteractionPrompt"},
    {"name": "textStyle", "type": "String", "default": "Text.InteractionPrompt"}
  ],
  "variants": [
    {
      "name": "keyboard",
      "displayName": "Keyboard",
      "parameters": {"message": "Press E to interact"}
    },
    {
      "name": "controller",
      "displayName": "Controller",
      "parameters": {"message": "Press A to interact"}
    }
  ],
  "root": {
    "id": "prompt",
    "type": "Panel",
    "styleClass": "{{style}}",
    "layout": {
      "type": "Stack",
      "direction": "Vertical",
      "padding": {"left": {"unit": "Normalized", "value": 0.020}, "right": {"unit": "Normalized", "value": 0.020}, "top": {"unit": "Normalized", "value": 0.010}, "bottom": {"unit": "Normalized", "value": 0.010}},
      "constraints": {
        "preferredWidth": {"unit": "Normalized", "value": 0.42},
        "preferredHeight": {"unit": "Normalized", "value": 0.11}
      }
    },
    "semantics": {"role": "Status", "name": "{{message}}", "nameKey": "interaction_prompt.label"},
    "children": [
      {
        "id": "label",
        "type": "Label",
        "text": "{{message}}",
        "textKey": "interaction_prompt.label",
        "styleClass": "{{textStyle}}",
        "horizontalAlign": "Center",
        "verticalAlign": "Middle",
        "layout": {
          "constraints": {
            "preferredHeight": {"unit": "Normalized", "value": 0.060}
          }
        },
        "semantics": {"role": "Label", "name": "{{message}}", "nameKey": "interaction_prompt.label"}
      }
    ]
  }
})json");
    }

    UiPackageDesc createVisualUiEditorEngineUiPackage()
    {
        UiWidgetAssetDefinition statusPrompt = createVisualUiEditorStatusPromptAsset();

        UiPackageDesc package;
        package.manifest.id = "engine.ui";
        package.manifest.displayName = "Engine UI";
        package.manifest.author = "Gravitas";
        package.manifest.version = "1.0.0";
        package.manifest.namespaceId = "engine";
        package.manifest.description = "Built-in engine UI widgets used by tool and game samples.";
        package.manifest.tags = {"engine", "ui", "widgets"};

        UiPackageAssetDesc asset;
        asset.reference = UiAssetReference{UiAssetType::WidgetAsset, statusPrompt.id};
        asset.widgetAsset = std::move(statusPrompt);
        package.assets.push_back(std::move(asset));

        UiPackageAssetDesc localization;
        localization.reference = UiAssetReference{UiAssetType::Localization, "engine.ui.locale.en-US"};
        localization.localizationAsset = engineLocalizationCatalog();
        package.assets.push_back(std::move(localization));
        return package;
    }

    UiPackageDesc createVisualUiEditorGameUiPackage()
    {
        UiWidgetAssetDefinition interactionPrompt = createVisualUiEditorInteractionPromptAsset();

        UiPackageDesc package;
        package.manifest.id = "game.ui";
        package.manifest.displayName = "Game UI";
        package.manifest.author = "DungeonCrawler";
        package.manifest.version = "1.0.0";
        package.manifest.namespaceId = "dungeon";
        package.manifest.description = "Game-authored UI widgets used by the Visual UI Editor sample.";
        package.manifest.tags = {"game", "ui", "widgets"};
        package.manifest.dependencies.push_back(UiPackageDependency{.packageId = "engine.ui", .minVersion = "1.0.0"});

        UiPackageAssetDesc asset;
        asset.reference = UiAssetReference{UiAssetType::WidgetAsset, interactionPrompt.id};
        asset.widgetAsset = std::move(interactionPrompt);
        package.assets.push_back(std::move(asset));

        UiPackageAssetDesc localization;
        localization.reference = UiAssetReference{UiAssetType::Localization, "game.ui.locale.en-US"};
        localization.localizationAsset = gameLocalizationCatalog();
        package.assets.push_back(std::move(localization));
        return package;
    }
}
