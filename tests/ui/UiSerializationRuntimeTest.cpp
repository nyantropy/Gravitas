#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>

#include "UiSerialization.h"
#include "UiSystem.h"

namespace
{
    void require(bool condition, const std::string& message)
    {
        if (!condition)
        {
            std::cerr << "UI serialization runtime test failed: " << message << std::endl;
            std::exit(1);
        }
    }

    const char* serializedAssetJson()
    {
        return R"json({
  "schema": 1,
  "id": "test.panel",
  "theme": "TestTheme",
  "surface": {
    "name": "serialized",
    "kind": "Screen",
    "order": 4,
    "rect": [0.0, 0.0, 1.0, 1.0]
  },
  "layers": [
    {"name": "hud", "order": 20, "visible": true, "inputEnabled": true}
  ],
  "root": {
    "id": "stack",
    "type": "Stack",
    "visible": true,
    "layout": {
      "mode": "Stack",
      "position": "Anchored",
      "width": "FromAnchors",
      "height": "FromAnchors",
      "anchorMin": [0.0, 0.0],
      "anchorMax": [1.0, 1.0],
      "gap": 0.05,
      "stackAxis": "Vertical",
      "mainAxisAlignment": "Start",
      "crossAxisAlignment": "Stretch"
    },
    "children": [
      {
        "id": "title",
        "type": "Label",
        "styleClass": "Text.Body",
        "text": "Ready",
        "verticalAlign": "Middle",
        "layout": {
          "constraints": {
            "preferredWidth": {"unit": "Normalized", "value": 0.6},
            "preferredHeight": {"unit": "Normalized", "value": 0.1}
          }
        },
        "semantics": {
          "role": "Status",
          "name": "Status",
          "liveRegion": "Polite"
        },
        "bindings": [
          {"property": "Text", "path": "status.text"}
        ]
      },
      {
        "id": "apply",
        "type": "Button",
        "styleClass": "Button",
        "labelStyleClass": "Text.Body",
        "text": "Apply",
        "navigation": {
          "enabled": true,
          "role": "Button",
          "group": "main",
          "tabIndex": 1,
          "wrapNavigation": true
        },
        "dragSource": {
          "payloadType": "test/action",
          "payloadId": 42,
          "payloadLabel": "Apply",
          "startThreshold": 0.01
        },
        "dropTarget": {
          "acceptedPayloadTypes": ["test/action"]
        },
        "layout": {
          "constraints": {
            "preferredWidth": {"unit": "Normalized", "value": 0.4},
            "preferredHeight": {"unit": "Normalized", "value": 0.12}
          }
        }
      },
      {
        "id": "health",
        "type": "ProgressBar",
        "styleClass": "ProgressBar.Track",
        "value": 0.25,
        "layout": {
          "constraints": {
            "preferredWidth": {"unit": "Normalized", "value": 0.5},
            "preferredHeight": {"unit": "Normalized", "value": 0.08}
          }
        },
        "semantics": {
          "role": "ProgressBar",
          "name": "Health",
          "liveRegion": "Polite",
          "relationships": {
            "describedBy": ["title"],
            "controls": ["apply"]
          },
          "hasRange": true,
          "rangeMin": 0.0,
          "rangeMax": 1.0,
          "rangeValue": 0.25
        },
        "bindings": [
          {
            "property": "Progress",
            "path": "health.percent",
            "animation": {"duration": 0.2, "ease": "Linear"}
          }
        ]
      }
    ]
  }
})json";
    }

    UiTheme testTheme()
    {
        UiTheme theme = defaultUiTheme();
        theme.setStyleClass("Text.Body", UiStyleClass{});
        theme.setStyleClass("Button", UiStyleClass{});
        theme.setStyleClass("ProgressBar.Track", UiStyleClass{});
        theme.setStyleClass("ProgressBar.Fill", UiStyleClass{});
        return theme;
    }

    const UiTextData& textPayload(UiSystem& ui, UiHandle handle)
    {
        const UiNode* node = ui.findNode(handle);
        require(node != nullptr && node->type == UiNodeType::Text, "text node missing");
        return std::get<UiTextData>(node->payload);
    }

    const UiSemanticNode* findSemantic(const UiSemanticTree& tree,
                                       UiSemanticRole role,
                                       const std::string& name)
    {
        for (const UiSemanticNode& node : tree.nodes)
        {
            if (node.role == role && node.name == name)
                return &node;
        }
        return nullptr;
    }

    bool containsHandle(const std::vector<UiHandle>& handles, UiHandle target)
    {
        return std::find(handles.begin(), handles.end(), target) != handles.end();
    }

    class TestBindingResolver final : public IUiSerializedBindingResolver
    {
    public:
        TestBindingResolver(UiObservable<std::string>& status,
                            UiObservable<double>& health)
        {
            sources["status.text"] = bindObservable(status);
            sources["health.percent"] = bindObservable(health);
        }

        std::optional<UiBindingSource> resolveBindingSource(const std::string& path) const override
        {
            const auto it = sources.find(path);
            if (it == sources.end())
                return std::nullopt;
            return it->second;
        }

    private:
        std::unordered_map<std::string, UiBindingSource> sources;
    };

    void testParseValidateAndRoundTrip()
    {
        UiSerializedAsset asset;
        UiSerializedValidationResult parseResult;
        require(parseUiSerializedAsset(serializedAssetJson(), asset, &parseResult),
                "asset did not parse");
        require(parseResult.valid(), "parsed asset did not validate");
        require(asset.id == "test.panel", "asset id mismatch");
        require(asset.surface.has_value() && asset.surface->name == "serialized",
                "surface descriptor did not parse");
        require(asset.root.children.size() == 3, "widget tree did not parse");
        require(asset.root.children[0].semantics.role == UiSemanticRole::Status,
                "semantic role did not parse");
        require(asset.root.children[2].bindings[0].animation.has_value(),
                "binding animation hint did not parse");
        require(asset.root.children[2].hasSemanticRelationships &&
                    asset.root.children[2].semanticRelationships.describedBy.size() == 1 &&
                    asset.root.children[2].semanticRelationships.describedBy[0] == "title",
                "durable semantic relationship ids did not parse");

        UiTheme theme = testTheme();
        UiSerializedValidationResult validation = validateUiSerializedAsset(asset, &theme);
        require(validation.valid(), "asset failed theme validation");

        const std::string serialized = serializeUiSerializedAsset(asset);
        UiSerializedAsset reparsed;
        require(parseUiSerializedAsset(serialized, reparsed), "round-trip asset did not parse");
        require(reparsed.root.children[1].navigation.enabled, "navigation metadata did not round-trip");
        require(reparsed.root.children[1].dragSource.has_value(), "drag source did not round-trip");
        require(reparsed.root.children[2].semantics.name == "Health", "accessibility metadata did not round-trip");
        require(reparsed.root.children[2].hasSemanticRelationships &&
                    reparsed.root.children[2].semanticRelationships.controls.size() == 1 &&
                    reparsed.root.children[2].semanticRelationships.controls[0] == "apply",
                "durable semantic relationship ids did not round-trip");
    }

    void testVersionMigrationAndValidation()
    {
        const std::string migratedJson =
            R"json({"schema":0,"id":"old","root":{"id":"label","type":"Label","text":"old"}})json";
        UiSerializedAsset migrated;
        UiSerializedValidationResult migration;
        require(parseUiSerializedAsset(migratedJson, migrated, &migration),
                "schema migration parse failed");
        require(migrated.schemaVersion == UI_SERIALIZATION_SCHEMA_VERSION,
                "schema migration did not update version");
        require(std::any_of(migration.issues.begin(),
                            migration.issues.end(),
                            [](const UiSerializedValidationIssue& issue)
                            {
                                return issue.severity == UiSerializedValidationIssue::Severity::Warning;
                            }),
                "schema migration did not report warning");

        UiSerializedAsset invalid;
        require(parseUiSerializedAsset(
                    R"json({"schema":1,"id":"bad","root":{"id":"x","type":"Missing","styleClass":"MissingStyle","layout":{"gridColumns":0}}})json",
                    invalid) == false,
                "invalid unknown widget was accepted");

        UiTheme theme = testTheme();
        invalid.schemaVersion = 1;
        invalid.root.id = "x";
        invalid.root.type = "Label";
        invalid.root.styleClass = "MissingStyle";
        UiSerializedValidationResult validation = validateUiSerializedAsset(invalid, &theme);
        require(!validation.valid(), "missing style class was accepted");

        UiSerializedAsset invalidRelationship;
        require(parseUiSerializedAsset(
                    R"json({"schema":1,"id":"bad.relationship","root":{"id":"label","type":"Label","semantics":{"role":"Label","name":"Label","relationships":{"describedBy":["missing"]}}}})json",
                    invalidRelationship) == false,
                "unknown semantic relationship id was accepted");
    }

    void testInstantiateRuntimeGraph()
    {
        UiSerializedAsset asset;
        require(parseUiSerializedAsset(serializedAssetJson(), asset), "asset parse failed");

        UiSystem ui(nullptr);
        ui.setTheme(testTheme());
        const UiMountId mount = ui.createMount();
        UiObservable<std::string> status("Bound ready");
        UiObservable<double> health(0.40);
        TestBindingResolver resolver(status, health);

        UiSerializedLoadResult result = ui.instantiateUiAsset(asset, mount, &resolver);
        require(result.success, "serialized asset did not instantiate");
        require(result.instance.root != UI_INVALID_HANDLE, "serialized root missing");
        require(result.instance.handles.count("title") == 1, "title id missing");
        require(result.instance.handles.count("apply") == 1, "button id missing");
        require(result.instance.handles.count("health.fill") == 1, "progress fill id missing");

        require(textPayload(ui, result.instance.handles.at("title")).text == "Bound ready",
                "text binding did not apply during instantiation");
        const UiNavigationNode* nav =
            ui.navigationGraph().findNode(result.instance.handles.at("apply"));
        require(nav != nullptr && nav->desc.group == "main" && nav->desc.tabIndex == 1,
                "navigation metadata was not registered");
        const UiDragSource* drag =
            ui.dragDropManager().findSource(result.instance.handles.at("apply"));
        require(drag != nullptr && drag->desc.payload.type == "test/action",
                "drag source metadata was not registered");
        const UiDropTarget* drop =
            ui.dragDropManager().findTarget(result.instance.handles.at("apply"));
        require(drop != nullptr && !drop->desc.acceptedPayloadTypes.empty(),
                "drop target metadata was not registered");

        ui.updateAccessibility();
        UiSemanticTree tree = ui.accessibilityTree();
        const UiSemanticNode* progress = findSemantic(tree, UiSemanticRole::ProgressBar, "Health");
        require(progress != nullptr && progress->hasRange && std::abs(progress->rangeValue - 0.40f) < 0.001f,
                "progress semantic range did not bind");

        ui.drainAccessibilityAnnouncements();
        status.set("Updated");
        health.set(0.75);
        ui.updateBindings();
        require(textPayload(ui, result.instance.handles.at("title")).text == "Updated",
                "text binding did not update");
        tree = ui.accessibilityTree();
        progress = findSemantic(tree, UiSemanticRole::ProgressBar, "Health");
        require(progress != nullptr && std::abs(progress->rangeValue - 0.75f) < 0.001f,
                "progress semantic range did not update");
        require(containsHandle(progress->relationships.describedBy, result.instance.handles.at("title")) &&
                    containsHandle(progress->relationships.controls, result.instance.handles.at("apply")),
                "semantic relationship ids did not resolve to runtime handles");
        const std::vector<UiAccessibilityAnnouncement> announcements =
            ui.drainAccessibilityAnnouncements();
        require(!announcements.empty(), "live region binding did not announce");
    }

    void testFileSaveAndLoad()
    {
        UiSerializedAsset asset;
        require(parseUiSerializedAsset(serializedAssetJson(), asset), "asset parse failed");

        const std::filesystem::path path =
            std::filesystem::temp_directory_path() / "gravitas_ui_serialization_test.ui.json";
        std::string error;
        require(saveUiSerializedAssetToFile(path.string(), asset, &error), "asset save failed: " + error);

        UiSerializedAsset loaded;
        UiSerializedValidationResult validation;
        require(loadUiSerializedAssetFromFile(path.string(), loaded, &validation), "asset load failed");
        require(loaded.id == asset.id && loaded.root.children.size() == asset.root.children.size(),
                "loaded asset did not match saved asset");
    }
}

int main()
{
    testParseValidateAndRoundTrip();
    testVersionMigrationAndValidation();
    testInstantiateRuntimeGraph();
    testFileSaveAndLoad();
    return 0;
}
