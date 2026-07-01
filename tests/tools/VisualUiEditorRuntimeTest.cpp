#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <variant>
#include <vector>

#include "UiSystem.h"
#include "VisualUiEditorDocument.h"
#include "VisualUiEditorSamples.h"

namespace
{
    void require(bool condition, const std::string& message)
    {
        if (!condition)
        {
            std::cerr << "Visual UI editor runtime test failed: " << message << std::endl;
            std::exit(1);
        }
    }

    UiTheme editorPreviewTheme()
    {
        UiTheme theme = defaultUiTheme();
        gts::tools::registerVisualUiEditorSampleThemeClasses(theme);
        return theme;
    }

    void registerSampleDependencies(UiSystem& ui)
    {
        UiPackageLoadResult enginePackage =
            ui.packages().registerPackage(ui, gts::tools::createVisualUiEditorEngineUiPackage());
        require(enginePackage.success, "engine UI sample package did not register");
        UiPackageLoadResult gamePackage =
            ui.packages().registerPackage(ui, gts::tools::createVisualUiEditorGameUiPackage());
        require(gamePackage.success, "game UI sample package did not register");
    }

    std::string validationSummary(const UiSerializedValidationResult& validation)
    {
        std::string summary;
        for (const UiSerializedValidationIssue& issue : validation.issues)
        {
            if (!summary.empty())
                summary += "; ";
            summary += issue.path + ": " + issue.message;
        }
        return summary.empty() ? "no validation issues" : summary;
    }

    std::string loadSummary(const UiSerializedLoadResult& result)
    {
        return "root=" + std::to_string(result.instance.root) +
               " handles=" + std::to_string(result.instance.handles.size()) +
               " validation=" + validationSummary(result.validation);
    }

    const UiTextData& textPayload(UiSystem& ui, UiSurfaceId surface, UiHandle handle)
    {
        const UiNode* node = ui.findNode(surface, handle);
        require(node != nullptr && node->type == UiNodeType::Text, "text node missing");
        return std::get<UiTextData>(node->payload);
    }

    gts::tools::VisualUiEditorDocument openPromptDocument()
    {
        gts::tools::VisualUiEditorDocument document;
        require(document.openAsset(gts::tools::createVisualUiEditorInteractionPromptAsset()),
                "interaction prompt asset did not open");
        return document;
    }

    void testOpenSelectionAndAuthoredEdits()
    {
        gts::tools::VisualUiEditorDocument document = openPromptDocument();

        std::vector<gts::tools::VisualUiEditorHierarchyItem> hierarchy = document.hierarchy();
        require(hierarchy.size() == 2, "prompt hierarchy should contain panel and label");
        require(hierarchy[0].widgetId == "prompt", "root prompt id missing");
        require(hierarchy[1].widgetId == "label", "label child id missing");

        require(document.selectWidget("label"), "stable widget id selection failed");
        require(document.setSelectedText("Press F to inspect"), "selected text edit failed");
        require(document.setSelectedStyleClass("Text.EditorPreview"), "selected style edit failed");
        require(document.dirty(), "document did not mark dirty after authored edit");

        std::vector<gts::tools::EditorPropertyDescriptor> properties = document.selectedProperties();
        bool foundText = false;
        bool foundStyle = false;
        for (const gts::tools::EditorPropertyDescriptor& property : properties)
        {
            if (property.metadata.id == "widget.text")
                foundText = property.value.textValue == "Press F to inspect";
            if (property.metadata.id == "widget.styleClass")
                foundStyle = property.value.textValue == "Text.EditorPreview";
        }
        require(foundText, "inspector properties did not expose edited text");
        require(foundStyle, "inspector properties did not expose edited style");

        require(document.renameSelectedWidget("promptLabel"), "stable id rename failed");
        require(document.selectedWidgetId() == "promptLabel", "selection did not follow renamed authored id");
        require(!document.renameSelectedWidget("prompt"), "duplicate widget id rename should be rejected");
    }

    void testValidationPreviewSurfaceAndCleanup()
    {
        UiSystem ui(nullptr);
        ui.setTheme(editorPreviewTheme());
        registerSampleDependencies(ui);

        gts::tools::VisualUiEditorDocument document = openPromptDocument();
        UiSerializedValidationResult validation = document.validate(ui.widgetAssets());
        require(validation.valid(), "valid interaction prompt reported validation errors");
        UiSerializedValidationResult registrationValidation;
        require(ui.widgetAssets().registerAsset(document.asset(), &registrationValidation),
                "interaction prompt did not register before preview: " + validationSummary(registrationValidation));
        UiWidgetAssetInstanceDesc expandDesc;
        expandDesc.assetId = document.asset().id;
        UiWidgetAssetExpandResult expanded = ui.widgetAssets().expandAsset(expandDesc);
        require(expanded.success, "interaction prompt did not expand before preview: " +
                                      validationSummary(expanded.validation));
        require(expanded.widget.id == "prompt" && expanded.widget.type == "Panel",
                "expanded interaction prompt root mismatch: " + expanded.widget.id + "/" + expanded.widget.type);
        UiSurfaceDesc directSurfaceDesc;
        directSurfaceDesc.name = "visual-ui-editor-direct-preview";
        UiSurfaceId directSurface = ui.createSurface(directSurfaceDesc);
        ui.setTheme(directSurface, ui.theme());
        UiMountId directMount = ui.createMount(directSurface);
        UiSerializedAsset directAsset;
        directAsset.id = "direct.preview";
        directAsset.root = expanded.widget;
        UiSerializedLoadResult directLoad =
            UiSerializationRuntime::instantiate(ui, directSurface, directMount, directAsset, nullptr, ui.theme(directSurface), &ui.widgetAssets());
        require(directLoad.success, "direct serializer preview failed: " + loadSummary(directLoad));
        ui.destroySurface(directSurface);

        require(document.rebuildPreview(ui),
                "preview rebuild failed: " + loadSummary(document.preview().loadResult));
        const gts::tools::VisualUiEditorPreviewState firstPreview = document.preview();
        require(firstPreview.surface != UI_INVALID_SURFACE, "preview surface missing");
        require(firstPreview.surface != ui.getDefaultSurface(), "preview reused the default surface");
        require(firstPreview.mount != UI_INVALID_MOUNT, "preview mount missing");
        require(firstPreview.loadResult.instance.handles.count("prompt") == 1, "preview root handle missing");
        require(firstPreview.loadResult.instance.handles.count("label") == 1, "preview label handle missing");

        UiHandle oldRoot = firstPreview.loadResult.instance.handles.at("prompt");
        require(document.selectWidget("label"), "label selection before rebuild failed");
        require(document.setSelectedText("Edited in editor"), "authored edit before rebuild failed");
        require(document.rebuildPreview(ui),
                "preview rebuild after edit failed: " + loadSummary(document.preview().loadResult));
        const gts::tools::VisualUiEditorPreviewState secondPreview = document.preview();
        require(secondPreview.rebuildCount == firstPreview.rebuildCount + 1, "preview rebuild count did not advance");
        require(ui.findNode(secondPreview.surface, oldRoot) == nullptr, "old preview subtree survived rebuild");
        require(document.selectedWidgetId() == "label", "preview rebuild did not preserve stable selection id");

        document.setPreviewVisible(ui, false);
        const UiSurface* surface = ui.findSurface(secondPreview.surface);
        require(surface != nullptr && !surface->isVisible() && !surface->inputEnabled(),
                "preview visibility did not disable the preview surface");

        document.destroyPreview(ui);
        require(ui.findSurface(secondPreview.surface) == nullptr, "preview surface did not clean up");
    }

    void testSaveRoundTripAndInvalidValidation()
    {
        UiSystem ui(nullptr);
        ui.setTheme(editorPreviewTheme());
        registerSampleDependencies(ui);

        gts::tools::VisualUiEditorDocument document = openPromptDocument();
        require(document.selectWidget("label"), "label selection for save test failed");
        require(document.setSelectedText("Saved prompt text"), "text edit for save test failed");

        const std::filesystem::path path =
            std::filesystem::temp_directory_path() / "gravitas_visual_ui_editor_runtime_test.uiwidget.json";
        std::string error;
        require(document.saveAs(path, &error), "visual UI editor document save failed: " + error);
        require(!document.dirty(), "document remained dirty after save");

        UiWidgetAssetDefinition loaded;
        UiSerializedValidationResult validation;
        require(loadUiWidgetAssetFromFile(path.string(), loaded, &validation), "saved widget asset did not load");
        require(loaded.id == "dungeon.interaction_prompt", "saved widget asset id mismatch");
        require(loaded.root.children.size() == 1 && loaded.root.children[0].text == "Saved prompt text",
                "saved authored widget edit did not round-trip");

        loaded.baseAsset = "missing.base.asset";
        gts::tools::VisualUiEditorDocument invalidDocument;
        require(invalidDocument.openAsset(loaded), "invalid document did not open");
        UiSerializedValidationResult invalid = invalidDocument.validate(ui.widgetAssets());
        require(!invalid.valid(), "missing base asset did not validate as an error");
    }

    void testSaveTriggersLivePreviewReload()
    {
        UiSystem ui(nullptr);
        ui.setTheme(editorPreviewTheme());
        registerSampleDependencies(ui);

        gts::tools::VisualUiEditorDocument document = openPromptDocument();
        UiSerializedValidationResult registrationValidation;
        require(ui.widgetAssets().registerAsset(document.asset(), &registrationValidation),
                "interaction prompt did not register before live reload preview");
        require(document.rebuildPreview(ui), "initial live reload preview failed");
        const uint32_t initialRebuildCount = document.preview().rebuildCount;
        require(document.selectWidget("label"), "label selection for live reload failed");
        require(document.setSelectedText("Saved through hot reload"), "live reload text edit failed");

        const std::filesystem::path path =
            std::filesystem::temp_directory_path() / "gravitas_visual_ui_editor_live_reload_test.uiwidget.json";
        std::string error;
        require(document.saveAsAndReload(ui, path, &error), "save and live reload failed: " + error);

        const gts::tools::VisualUiEditorPreviewState preview = document.preview();
        require(preview.rebuildCount == initialRebuildCount + 1,
                "live reload did not advance preview rebuild count");
        require(preview.loadResult.instance.handles.count("label") == 1,
                "live reload preview label handle missing");
        require(textPayload(ui, preview.surface, preview.loadResult.instance.handles.at("label")).text ==
                    "Saved through hot reload",
                "live reload preview did not reflect saved authored text");
        require(document.selectedWidgetId() == "label",
                "live reload did not preserve editor stable-id selection");
    }
}

int main()
{
    testOpenSelectionAndAuthoredEdits();
    testValidationPreviewSurfaceAndCleanup();
    testSaveRoundTripAndInvalidValidation();
    testSaveTriggersLivePreviewReload();
    return 0;
}
