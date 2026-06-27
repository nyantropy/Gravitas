#include <filesystem>
#include <iostream>
#include <string>

#include "EditorCommands.h"
#include "EditorLayout.h"
#include "EditorPropertySystem.h"
#include "EditorWidgets.h"
#include "EditorWorkspace.h"
#include "UiSystem.h"

namespace
{
    void require(bool condition, const std::string& message)
    {
        if (!condition)
        {
            std::cerr << "Editor framework smoke test failed: " << message << std::endl;
            std::exit(1);
        }
    }
} // namespace

int main()
{
    UiSystem ui(nullptr);

    gts::tools::EditorPanelState hierarchy;
    hierarchy.id       = "hierarchy";
    hierarchy.title    = "Hierarchy";
    hierarchy.subtitle = "Scene";
    hierarchy.area     = gts::tools::EditorDockArea::Left;
    hierarchy.size     = 0.22f;

    gts::tools::EditorPanelState inspector;
    inspector.id       = "inspector";
    inspector.title    = "Inspector";
    inspector.subtitle = "Selection";
    inspector.area     = gts::tools::EditorDockArea::Right;
    inspector.size     = 0.28f;
    inspector.closable = true;

    gts::tools::EditorPanelState workspace;
    workspace.id       = "workspace";
    workspace.title    = "Workspace";
    workspace.subtitle = "Graph";
    workspace.area     = gts::tools::EditorDockArea::Bottom;
    workspace.size     = 0.24f;

    gts::tools::EditorDockLayoutSpec spec;
    spec.toolbarTitle  = "GRAVITAS TEST";
    spec.footerStatus  = "READY";
    spec.panels        = {hierarchy, inspector, workspace};
    spec.workspaceTabs = {{"curves", "Curves", false, true, false},
                          {"graph", "Graph", true, true, false},
                          {"diagnostics", "Diagnostics", false, true, false}};

    const gts::tools::EditorDockLayoutHandles handles =
        gts::tools::buildEditorDockLayout(ui, ui.getRoot(), nullptr, spec);

    require(handles.root != UI_INVALID_HANDLE, "dock root was not created");
    require(handles.toolbar.root != UI_INVALID_HANDLE, "toolbar was not created");
    require(handles.sidebar.root != UI_INVALID_HANDLE, "sidebar was not created");
    require(handles.footer.root != UI_INVALID_HANDLE, "footer was not created");
    require(handles.center != UI_INVALID_HANDLE, "center surface was not created");
    require(handles.panels.size() == 3u, "expected three docked panels");
    require(handles.workspaceTabs.tabs.size() == 3u, "expected three workspace tabs");
    require(handles.centerRect.width > 0.0f && handles.centerRect.height > 0.0f, "center rect collapsed");

    gts::tools::collapsePanel(workspace);
    require(workspace.collapsed, "collapse did not update state");
    require(workspace.restoredSize == 0.24f, "collapse did not preserve restored size");

    gts::tools::setPanelSize(workspace, 2.0f);
    require(workspace.size <= workspace.maxSize, "panel size was not clamped");

    gts::tools::hidePanel(workspace);
    require(!workspace.visible, "hide did not update visibility");

    gts::tools::restorePanel(workspace);
    require(workspace.visible && !workspace.collapsed, "restore did not show and expand panel");

    gts::tools::EditorSplitViewSpec split;
    split.orientation = gts::tools::EditorSplitOrientation::Vertical;
    const gts::tools::EditorSplitViewHandles splitHandles =
        gts::tools::createEditorSplitView(ui, handles.center, {0.0f, 0.10f, 1.0f, 0.80f}, split);
    require(splitHandles.first != UI_INVALID_HANDLE, "split first pane was not created");
    require(splitHandles.splitter != UI_INVALID_HANDLE, "splitter was not created");
    require(splitHandles.second != UI_INVALID_HANDLE, "split second pane was not created");

    const std::vector<gts::tools::EditorTreeNodeSpec> treeNodes = {{"scene", "Scene", "root", 0, true, true, true},
                                                                   {"camera", "Camera", "entity", 1, true, false, true},
                                                                   {"light", "Light", "entity", 1, true, false, true}};
    const gts::tools::EditorWidgetHandles             tree =
        gts::tools::createEditorHierarchyView(ui, splitHandles.first, {0.0f, 0.0f, 1.0f, 0.45f}, treeNodes, nullptr);
    require(tree.surfaces.size() == 3u, "hierarchy rows were not created");

    const std::vector<gts::tools::EditorPropertySpec> properties = {
        {"name", "Name", "Camera", "", "General", gts::tools::EditorPropertyWidgetKind::Text, false, false, true},
        {"fov", "FOV", "60", "", "Camera", gts::tools::EditorPropertyWidgetKind::Float, false, true, true},
        {"mesh", "Mesh", "cube.obj", "", "Assets", gts::tools::EditorPropertyWidgetKind::Asset, true, false, true}};
    const gts::tools::EditorWidgetHandles inspectorWidget = gts::tools::createEditorInspector(
        ui, splitHandles.second, {0.0f, 0.0f, 1.0f, 0.45f}, "Inspector", properties, nullptr);
    require(inspectorWidget.buttons.size() == 3u, "property grid values were not created");

    const std::vector<gts::tools::EditorMenuItemSpec> menuItems = {{"copy", "Copy", "Ctrl+C", true, false, false},
                                                                   {"separator", "", "", true, false, true},
                                                                   {"delete", "Delete", "Del", true, false, false}};
    const gts::tools::EditorWidgetHandles             popup =
        gts::tools::createEditorPopupMenu(ui, handles.center, {0.10f, 0.10f, 0.24f, 0.24f}, menuItems, nullptr);
    require(popup.buttons.size() == 2u, "popup menu buttons were not created");

    const std::vector<gts::tools::EditorAssetSpec> assets = {
        {"texture", "engine_ui_fallback.png", "Texture", true, true}, {"mesh", "cube.obj", "Mesh", false, true}};
    const gts::tools::EditorWidgetHandles assetPicker =
        gts::tools::createEditorAssetPicker(ui, handles.center, {0.36f, 0.10f, 0.28f, 0.30f}, assets, nullptr, "cube");
    require(assetPicker.fields.size() == 1u, "asset picker search field was not created");

    const gts::tools::EditorWidgetHandles range = gts::tools::createEditorDualRangeSlider(
        ui, handles.center, {0.66f, 0.10f, 0.25f, 0.08f}, "Range", 0.0f, 1.0f, 0.2f, 0.8f, nullptr);
    require(range.surfaces.size() >= 2u, "dual range slider surfaces were not created");

    const gts::tools::EditorWidgetHandles color = gts::tools::createEditorColorPicker(
        ui, handles.center, {0.66f, 0.20f, 0.25f, 0.18f}, {0.3f, 0.5f, 0.7f, 1.0f}, nullptr);
    require(color.sliders.size() == 3u, "color picker sliders were not created");

    const std::vector<gts::tools::EditorGradientKeySpec> gradientKeys = {{0.0f, {1.0f, 0.2f, 0.2f, 1.0f}, false},
                                                                         {1.0f, {0.2f, 0.5f, 1.0f, 1.0f}, true}};
    const gts::tools::EditorWidgetHandles                gradient =
        gts::tools::createEditorGradientEditor(ui, handles.center, {0.10f, 0.48f, 0.25f, 0.10f}, gradientKeys);
    require(gradient.surfaces.size() == 3u, "gradient editor keys were not created");

    const std::vector<gts::tools::EditorCurveKeySpec> curveKeys = {{0.0f, 0.0f, false}, {0.5f, 1.0f, true}};
    const gts::tools::EditorWidgetHandles             curve =
        gts::tools::createEditorCurveEditor(ui, handles.center, {0.38f, 0.48f, 0.25f, 0.18f}, curveKeys);
    require(curve.surfaces.size() >= 8u, "curve editor primitives were not created");

    const std::vector<gts::tools::EditorTimelineTrackSpec> tracks = {{"position", "Position", {0.1f, 0.8f}},
                                                                     {"opacity", "Opacity", {0.3f}}};
    const gts::tools::EditorWidgetHandles                  timeline =
        gts::tools::createEditorTimeline(ui, handles.center, {0.66f, 0.42f, 0.25f, 0.22f}, tracks, nullptr);
    require(timeline.labels.size() == 2u, "timeline labels were not created");

    const std::vector<gts::tools::EditorGraphNodeSpec> graphNodes = {
        {"spawn", "Spawn", {0.08f, 0.20f, 0.20f, 0.16f}, true},
        {"render", "Render", {0.62f, 0.55f, 0.22f, 0.16f}, false}};
    const std::vector<gts::tools::EditorGraphLinkSpec> links = {{0u, 1u}};
    const gts::tools::EditorWidgetHandles              graph = gts::tools::createEditorGraphCanvas(
        ui, handles.center, {0.10f, 0.70f, 0.35f, 0.24f}, graphNodes, links, nullptr);
    require(graph.labels.size() == 2u, "graph node labels were not created");

    const std::vector<gts::tools::EditorValidationMessageSpec> messages = {
        {gts::tools::EditorValidationSeverity::Warning, "Graph", "Missing optional force node"},
        {gts::tools::EditorValidationSeverity::Error, "Asset", "Texture path is empty"}};
    const gts::tools::EditorWidgetHandles diagnostics =
        gts::tools::createEditorDiagnosticsPanel(ui, handles.center, {0.48f, 0.70f, 0.42f, 0.18f}, messages, nullptr);
    require(diagnostics.labels.size() == 4u, "diagnostics labels were not created");

    const gts::tools::EditorWidgetHandles console = gts::tools::createEditorConsoleOutput(
        ui, handles.center, {0.48f, 0.90f, 0.42f, 0.08f}, {"ready", "compiled"}, nullptr);
    require(console.labels.size() == 2u, "console output labels were not created");

    const gts::tools::EditorWidgetHandles palette = gts::tools::createEditorSearchPalette(
        ui, handles.center, {0.20f, 0.18f, 0.32f, 0.42f}, "save", menuItems, nullptr);
    require(palette.fields.size() == 1u && palette.buttons.size() == 2u, "search palette was not created");

    gts::tools::EditorPropertyDescriptor enabled;
    enabled.metadata.id           = "enabled";
    enabled.metadata.displayName  = "Enabled";
    enabled.metadata.description  = "Controls whether the feature runs.";
    enabled.metadata.type         = gts::tools::EditorPropertyValueType::Bool;
    enabled.metadata.defaultValue = gts::tools::boolPropertyValue(true);
    enabled.value                 = gts::tools::boolPropertyValue(true);

    gts::tools::EditorPropertyDescriptor rate;
    rate.metadata.id                = "rate";
    rate.metadata.displayName       = "Rate";
    rate.metadata.category          = "Spawn";
    rate.metadata.group             = "Emission";
    rate.metadata.type              = gts::tools::EditorPropertyValueType::Float;
    rate.metadata.defaultValue      = gts::tools::floatPropertyValue(10.0f);
    rate.metadata.limits.hasMin     = true;
    rate.metadata.limits.hasMax     = true;
    rate.metadata.limits.hasSoftMin = true;
    rate.metadata.limits.hasSoftMax = true;
    rate.metadata.limits.min        = 0.0f;
    rate.metadata.limits.max        = 200.0f;
    rate.metadata.limits.softMin    = 0.0f;
    rate.metadata.limits.softMax    = 60.0f;
    rate.metadata.limits.step       = 0.25f;
    rate.metadata.units             = "pps";
    rate.value                      = gts::tools::floatPropertyValue(24.0f);

    gts::tools::EditorPropertyDescriptor asset;
    asset.metadata.id           = "texture";
    asset.metadata.displayName  = "Texture";
    asset.metadata.type         = gts::tools::EditorPropertyValueType::Asset;
    asset.metadata.assetType    = "Texture";
    asset.metadata.readOnly     = true;
    asset.metadata.defaultValue = gts::tools::textPropertyValue(gts::tools::EditorPropertyValueType::Asset, "");
    asset.value = gts::tools::textPropertyValue(gts::tools::EditorPropertyValueType::Asset, "engine_ui_fallback.png");

    gts::tools::EditorPropertyDescriptor hidden;
    hidden.metadata.id                         = "hiddenWhenEnabled";
    hidden.metadata.displayName                = "Hidden";
    hidden.metadata.type                       = gts::tools::EditorPropertyValueType::Text;
    hidden.metadata.visibilityRule.dependsOn   = "enabled";
    hidden.metadata.visibilityRule.equalsValue = "false";
    hidden.metadata.defaultValue = gts::tools::textPropertyValue(gts::tools::EditorPropertyValueType::Text, "");
    hidden.value = gts::tools::textPropertyValue(gts::tools::EditorPropertyValueType::Text, "not shown");

    std::vector<gts::tools::EditorPropertyDescriptor> descriptors   = {enabled, rate, asset, hidden};
    const std::vector<gts::tools::EditorPropertySpec> metadataSpecs = gts::tools::buildPropertySpecs(descriptors);
    require(metadataSpecs.size() == 3u, "visibility rules did not hide metadata property");
    require(metadataSpecs[1].widget == gts::tools::EditorPropertyWidgetKind::Float, "float widget was not selected");
    require(metadataSpecs[1].modified, "modified metadata property was not detected");
    require(metadataSpecs[2].readOnly, "read-only metadata was not propagated");
    require(metadataSpecs[2].value.find("[Texture]") != std::string::npos, "asset type was not displayed");

    const gts::tools::EditorWidgetHandles metadataGrid =
        gts::tools::createMetadataPropertyGrid(ui, handles.center, {0.04f, 0.04f, 0.32f, 0.18f}, descriptors, nullptr);
    require(metadataGrid.buttons.size() == 3u, "metadata property grid did not create rows");

    int                               saveCount = 0;
    gts::tools::EditorCommandRegistry commands;
    gts::tools::registerStandardEditorCommands(commands);
    require(commands.hasCommand(gts::tools::EditorStandardCommands::Save), "standard save command was not registered");
    require(commands.commandForShortcut("editor.shortcut.save") != nullptr, "save shortcut was not mapped");
    require(commands.setHandler(gts::tools::EditorStandardCommands::Save,
                                [&](const gts::tools::EditorCommandContext& context)
                                {
                                    ++saveCount;
                                    return gts::tools::EditorCommandResult{context.workspaceId == "fx", "saved"};
                                }),
            "failed to set save handler");

    gts::tools::EditorCommandContext commandContext;
    commandContext.workspaceId                    = "fx";
    gts::tools::EditorCommandResult commandResult = commands.routeShortcut("editor.shortcut.save", commandContext);
    require(commandResult.handled && saveCount == 1, "shortcut did not execute save command");

    commands.setEnabled(gts::tools::EditorStandardCommands::Save, false);
    commandResult = commands.execute(gts::tools::EditorStandardCommands::Save, commandContext);
    require(!commandResult.handled && saveCount == 1, "disabled command executed");

    int                         undoValue = 0;
    gts::tools::EditorUndoStack undoStack;
    undoStack.push({"Set Value",
                    [&]()
                    {
                        undoValue = 0;
                    },
                    [&]()
                    {
                        undoValue = 1;
                    }});
    require(undoStack.canUndo() && undoStack.undoLabel() == "Set Value", "undo stack did not expose undo state");
    undoValue = 1;
    require(undoStack.undo() && undoValue == 0, "undo callback did not run");
    require(undoStack.canRedo() && undoStack.redoLabel() == "Set Value", "redo state was not exposed");
    require(undoStack.redo() && undoValue == 1, "redo callback did not run");

    std::vector<gts::tools::EditorWorkspaceState> defaultWorkspaces = gts::tools::makeDefaultEditorWorkspaces();
    require(defaultWorkspaces.size() == 4u, "default editor workspaces were not created");

    gts::tools::EditorWorkspaceRegistry workspaceRegistry;
    for (gts::tools::EditorWorkspaceState& workspaceState : defaultWorkspaces)
        workspaceRegistry.addWorkspace(workspaceState);
    require(workspaceRegistry.setActiveWorkspace("fx"), "failed to activate fx workspace");
    require(workspaceRegistry.activeWorkspace() != nullptr &&
                workspaceRegistry.activeWorkspace()->displayName == "FX Editing",
            "active workspace was not retained");

    gts::tools::EditorWorkspaceState fxWorkspace = *workspaceRegistry.activeWorkspace();
    fxWorkspace.panels[0].size                   = 0.31f;
    fxWorkspace.panels[1].visible                = false;
    fxWorkspace.panels[2].collapsed              = true;
    fxWorkspace.selectedWorkspaceTabId           = "graph";

    const std::string                serializedWorkspace = gts::tools::serializeWorkspace(fxWorkspace);
    gts::tools::EditorWorkspaceState loadedWorkspace;
    require(gts::tools::deserializeWorkspace(serializedWorkspace, loadedWorkspace), "workspace did not deserialize");
    require(loadedWorkspace.id == "fx", "workspace id did not round trip");
    require(loadedWorkspace.panels.size() == fxWorkspace.panels.size(), "workspace panels did not round trip");
    require(!loadedWorkspace.panels[1].visible, "workspace panel visibility did not round trip");
    require(loadedWorkspace.panels[2].collapsed, "workspace panel collapse did not round trip");
    require(loadedWorkspace.selectedWorkspaceTabId == "graph", "workspace selected tab did not round trip");

    const std::filesystem::path workspacePath =
        std::filesystem::temp_directory_path() / "gravitas_editor_workspace_smoke.gtsws";
    require(gts::tools::saveWorkspaceFile(workspacePath, fxWorkspace), "workspace file save failed");
    gts::tools::EditorWorkspaceState fileWorkspace;
    require(gts::tools::loadWorkspaceFile(workspacePath, fileWorkspace), "workspace file load failed");
    require(fileWorkspace.toolbarItems.size() == fxWorkspace.toolbarItems.size(), "toolbar config did not round trip");
    std::filesystem::remove(workspacePath);

    const std::vector<gts::tools::EditorPanelState> layoutPanels = gts::tools::workspacePanelsToLayout(fileWorkspace);
    require(layoutPanels.size() == fileWorkspace.panels.size(), "workspace panels were not converted to layout panels");

    require(ui.getDocument().getNodeCount() > 120u, "editor framework smoke did not create enough retained nodes");
    return 0;
}
