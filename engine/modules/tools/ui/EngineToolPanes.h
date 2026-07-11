#pragma once

#include <algorithm>
#include <array>
#include <string>
#include <utility>

#include "EngineToolUiHelpers.h"
#include "ToolPane.h"
#include "ToolPaneWidgets.h"
#include "ToolPropertyInspector.h"
#include "ToolTheme.h"

namespace gts::tools
{
    class ToolPaneBase : public ToolPane
    {
    public:
        const PaneDescriptor& descriptor() const override { return paneDescriptor; }
        UiHandle root() const override { return rootPanel.root(); }

    protected:
        explicit ToolPaneBase(PaneDescriptor descriptor)
            : paneDescriptor(descriptor)
        {
        }

        void buildRoot(gts::ui::UiWidgetContext& context,
                       UiHandle parent,
                       BitmapFont* inFont,
                       const UiLayoutSpec& layout,
                       UiColor color,
                       bool interactable)
        {
            font = inFont;

            gts::ui::UiPanelDesc desc;
            desc.layout = layout;
            desc.styleClass.clear();
            desc.enabled = true;
            desc.interactable = interactable;
            rootPanel.build(context, parent, desc);
            toolui::setRectPayload(context.ui, context.surface, rootPanel.root(), color);
        }

        void updateRoot(gts::ui::UiWidgetContext& context, const UiLayoutSpec& layout, bool visible)
        {
            if (rootPanel.root() == UI_INVALID_HANDLE)
                return;

            context.ui.setLayout(context.surface, rootPanel.root(), layout);
            rootPanel.setVisible(context, visible);
        }

        void destroyRoot(gts::ui::UiWidgetContext& context)
        {
            rootPanel.destroy(context);
        }

        UiHandle content() const { return rootPanel.content(); }

        gts::ui::UiLabelDesc label(const std::string& text,
                                   const UiLayoutSpec& layout,
                                   UiColor color,
                                   float scale,
                                   UiHorizontalAlign align = UiHorizontalAlign::Left,
                                   int maxLines = 2) const
        {
            return toolLabel(font, text, layout, color, scale, align, maxLines);
        }

        gts::ui::UiButtonDesc button(const std::string& text,
                                     const UiLayoutSpec& layout,
                                     float scale,
                                     UiHorizontalAlign align = UiHorizontalAlign::Center) const
        {
            return toolButton(font, text, layout, scale, align);
        }

        static gts::ui::UiImageDesc image(const UiLayoutSpec& layout)
        {
            gts::ui::UiImageDesc desc;
            desc.layout = layout;
            desc.tint = {1.0f, 1.0f, 1.0f, 0.0f};
            desc.visible = false;
            return desc;
        }

        static void buildPanel(gts::ui::UiWidgetContext& context,
                               gts::ui::UiPanelWidget& panel,
                               UiHandle parent,
                               const UiLayoutSpec& layout,
                               UiColor color,
                               bool interactable = false)
        {
            gts::ui::UiPanelDesc desc;
            desc.layout = layout;
            desc.styleClass.clear();
            desc.enabled = true;
            desc.interactable = interactable;
            panel.build(context, parent, desc);
            toolui::setRectPayload(context.ui, context.surface, panel.root(), color);
        }

        void syncButton(gts::ui::UiWidgetContext& context,
                        gts::ui::UiButtonWidget& widget,
                        bool visible,
                        const std::string& text,
                        bool enabled,
                        bool active)
        {
            syncToolButton(context, widget, visible, text, enabled, active);
        }

        static std::string pageText(size_t offset, size_t visible, size_t total)
        {
            return toolPageText(offset, visible, total);
        }

        static UiColor transparent()
        {
            return {0.0f, 0.0f, 0.0f, 0.0f};
        }

        PaneDescriptor paneDescriptor;
        BitmapFont* font = nullptr;
        gts::ui::UiPanelWidget rootPanel;
    };

    class MenuBarPane final : public ToolPaneBase
    {
    public:
        MenuBarPane()
            : ToolPaneBase({ToolPaneId::MenuBar, ToolDockArea::MenuBar, "Menu", true, true, 0, 1.0f})
        {
        }

        void build(gts::ui::UiWidgetContext& context,
                   UiHandle parent,
                   BitmapFont* font,
                   const UiLayoutSpec& layout) override
        {
            buildRoot(context, parent, font, layout, transparent(), false);
            title.build(context,
                        content(),
                        label("GRAVITAS",
                              toolui::rect(0.014f, 0.120f, 0.088f, 0.740f),
                              ToolTheme::text,
                              ToolTheme::titleTextScale));
            menuItems.build(context,
                            content(),
                            label("File   Edit   View   Tools",
                                  toolui::rect(0.112f, 0.160f, 0.190f, 0.660f),
                                  ToolTheme::mutedText,
                                  ToolTheme::smallTextScale));
        }

        void update(gts::ui::UiWidgetContext& context,
                    const ToolShellView&,
                    const UiLayoutSpec& layout,
                    bool visible) override
        {
            updateRoot(context, layout, visible);
        }

        void onEvent(gts::ui::UiWidgetContext&, UiEvent&, ToolCommandQueue&, const ToolShellView&) override {}

        void destroy(gts::ui::UiWidgetContext& context) override
        {
            title.destroy(context);
            menuItems.destroy(context);
            destroyRoot(context);
        }

    private:
        gts::ui::UiLabelWidget title;
        gts::ui::UiLabelWidget menuItems;
    };

    class WorkspaceTabsPane final : public ToolPaneBase
    {
    public:
        WorkspaceTabsPane()
            : ToolPaneBase({ToolPaneId::WorkspaceTabs,
                            ToolDockArea::MenuBar,
                            "Workspaces",
                            true,
                            true,
                            1,
                            0.300f,
                            0.0f,
                            0.0f,
                            0.0f,
                            0.305f})
        {
        }

        void build(gts::ui::UiWidgetContext& context,
                   UiHandle parent,
                   BitmapFont* font,
                   const UiLayoutSpec& layout) override
        {
            buildRoot(context, parent, font, layout, transparent(), false);

            std::array<ToolToolbarButtonSlot, 2> slots;
            slots[0] = {"World", toolui::rect(0.000f, 0.160f, 0.480f, 0.680f)};
            slots[1] = {"Particles", toolui::rect(0.510f, 0.160f, 0.490f, 0.680f)};
            tabs.build(context, content(), font, slots, ToolTheme::buttonTextScale);
        }

        void update(gts::ui::UiWidgetContext& context,
                    const ToolShellView& view,
                    const UiLayoutSpec& layout,
                    bool visible) override
        {
            updateRoot(context, layout, visible);
            tabs.sync(context, 0, visible, "World", true, view.activeWorkspace == ToolWorkspace::World);
            tabs.sync(context, 1, visible, "Particles", true, view.activeWorkspace == ToolWorkspace::Particles);
        }

        void onEvent(gts::ui::UiWidgetContext& context,
                     UiEvent& event,
                     ToolCommandQueue& commands,
                     const ToolShellView&) override
        {
            tabs.onEvent(context, event);

            if (tabs.consumePressed(0))
                commands.push({ToolCommandType::SetWorkspace, ToolWorkspace::World});
            if (tabs.consumePressed(1))
                commands.push({ToolCommandType::SetWorkspace, ToolWorkspace::Particles});
        }

        void destroy(gts::ui::UiWidgetContext& context) override
        {
            tabs.destroy(context);
            destroyRoot(context);
        }

    private:
        ToolToolbarRow<2> tabs;
    };

    class ToolToolbarPane final : public ToolPaneBase
    {
    public:
        ToolToolbarPane()
            : ToolPaneBase({ToolPaneId::ToolToolbar, ToolDockArea::Toolbar, "Toolbar", true, true, 0})
        {
        }

        void build(gts::ui::UiWidgetContext& context,
                   UiHandle parent,
                   BitmapFont* font,
                   const UiLayoutSpec& layout) override
        {
            buildRoot(context, parent, font, layout, transparent(), false);
            buildPanel(context,
                       sceneGroup,
                       content(),
                       toolui::rect(0.012f, 0.120f, 0.318f, 0.760f),
                       ToolTheme::toolbarGroupBackground);
            viewportLabel.build(context,
                                sceneGroup.content(),
                                label("Scene",
                                      toolui::rect(0.040f, 0.080f, 0.205f, 0.840f),
                                      ToolTheme::mutedText,
                                      ToolTheme::smallTextScale));
            sceneLabel.build(context,
                             sceneGroup.content(),
                             label("Scene",
                                   toolui::rect(0.270f, 0.080f, 0.690f, 0.840f),
                                   ToolTheme::text,
                                   ToolTheme::smallTextScale));
            buildPanel(context,
                       actionGroup,
                       content(),
                       toolui::rect(0.348f, 0.115f, 0.340f, 0.770f),
                       ToolTheme::toolbarGroupBackground);
            std::array<ToolToolbarButtonSlot, 4> slots;
            slots[0] = {"Save", toolui::rect(0.025f, 0.150f, 0.170f, 0.700f)};
            slots[1] = {"Reload", toolui::rect(0.215f, 0.150f, 0.210f, 0.700f)};
            slots[2] = {"Play", toolui::rect(0.445f, 0.150f, 0.170f, 0.700f)};
            slots[3] = {"Restart", toolui::rect(0.635f, 0.150f, 0.340f, 0.700f)};
            actions.build(context, actionGroup.content(), font, slots, ToolTheme::buttonTextScale);
            buildPanel(context,
                       workspaceGroup,
                       content(),
                       toolui::rect(0.748f, 0.120f, 0.238f, 0.760f),
                       ToolTheme::toolbarGroupBackground);
            workspaceLabel.build(context,
                                 workspaceGroup.content(),
                                 label("Workspace",
                                       toolui::rect(0.040f, 0.080f, 0.920f, 0.840f),
                                       ToolTheme::mutedText,
                                       ToolTheme::smallTextScale,
                                       UiHorizontalAlign::Right));
        }

        void update(gts::ui::UiWidgetContext& context,
                    const ToolShellView& view,
                    const UiLayoutSpec& layout,
                    bool visible) override
        {
            updateRoot(context, layout, visible);
            const std::string activeScene = view.activeSceneName.empty() ? "none" : toolui::compact(view.activeSceneName, 48);
            sceneLabel.setText(context, activeScene);
            const bool particleWorkspace = view.activeWorkspace == ToolWorkspace::Particles;
            sceneGroup.setVisible(context, visible);
            actionGroup.setVisible(context, visible && particleWorkspace);
            workspaceGroup.setVisible(context, visible);
            viewportLabel.setVisible(context, visible);
            sceneLabel.setVisible(context, visible);
            actions.sync(context, 0, visible && particleWorkspace, "Save", view.particleLoaded, false);
            actions.sync(context, 1, visible && particleWorkspace, "Reload", view.particleLoaded, false);
            actions.sync(context, 2, visible && particleWorkspace, view.particlePlaying ? "Pause" : "Play", view.particleLoaded, false);
            actions.sync(context, 3, visible && particleWorkspace, "Restart", view.particleLoaded, false);
            workspaceLabel.setText(context,
                                   view.activeWorkspace == ToolWorkspace::Particles ? "Particles" : "World");
        }

        void onEvent(gts::ui::UiWidgetContext& context,
                     UiEvent& event,
                     ToolCommandQueue& commands,
                     const ToolShellView&) override
        {
            actions.onEvent(context, event);

            if (actions.consumePressed(0))
                commands.push({ToolCommandType::SaveParticleEffect});
            if (actions.consumePressed(1))
                commands.push({ToolCommandType::ReloadParticleEffect});
            if (actions.consumePressed(2))
                commands.push({ToolCommandType::ToggleParticlePlayback});
            if (actions.consumePressed(3))
                commands.push({ToolCommandType::RestartParticlePreview});
        }

        void destroy(gts::ui::UiWidgetContext& context) override
        {
            viewportLabel.destroy(context);
            sceneLabel.destroy(context);
            actions.destroy(context);
            workspaceLabel.destroy(context);
            workspaceGroup.destroy(context);
            actionGroup.destroy(context);
            sceneGroup.destroy(context);
            destroyRoot(context);
        }

    private:
        gts::ui::UiPanelWidget sceneGroup;
        gts::ui::UiLabelWidget viewportLabel;
        gts::ui::UiLabelWidget sceneLabel;
        gts::ui::UiPanelWidget actionGroup;
        ToolToolbarRow<4> actions;
        gts::ui::UiPanelWidget workspaceGroup;
        gts::ui::UiLabelWidget workspaceLabel;
    };

    class WorldViewportPane final : public ToolPaneBase
    {
    public:
        WorldViewportPane()
            : ToolPaneBase({ToolPaneId::WorldViewport, ToolDockArea::Center, "World Viewport", true, true, 0})
        {
        }

        void build(gts::ui::UiWidgetContext& context,
                   UiHandle parent,
                   BitmapFont* font,
                   const UiLayoutSpec& layout) override
        {
            buildRoot(context, parent, font, layout, transparent(), false);
            buildPanel(context,
                       topBar,
                       content(),
                       toolui::rect(0.000f, 0.000f, 1.000f, 0.054f),
                       ToolTheme::headerBackground,
                       true);
            buildPanel(context,
                       bottomBar,
                       content(),
                       toolui::rect(0.000f, 0.944f, 1.000f, 0.056f),
                       ToolTheme::headerBackground,
                       true);
            buildPanel(context,
                       evalChip,
                       content(),
                       toolui::rect(0.020f, 0.080f, 0.250f, 0.044f),
                       ToolTheme::viewportOverlay);
            buildPanel(context,
                       debugChip,
                       content(),
                       toolui::rect(0.740f, 0.080f, 0.240f, 0.044f),
                       ToolTheme::viewportOverlay);
            title.build(context,
                        topBar.content(),
                        label("World Viewport",
                              toolui::rect(0.018f, 0.080f, 0.220f, 0.840f),
                              ToolTheme::text,
                              ToolTheme::smallTextScale));
            scene.build(context,
                        topBar.content(),
                        label("Scene",
                              toolui::rect(0.260f, 0.080f, 0.490f, 0.840f),
                              ToolTheme::mutedText,
                              ToolTheme::smallTextScale));
            mode.build(context,
                       topBar.content(),
                       label("Runtime",
                             toolui::rect(0.780f, 0.080f, 0.200f, 0.840f),
                             ToolTheme::statusText,
                             ToolTheme::smallTextScale,
                             UiHorizontalAlign::Right));
            viewMode.build(context,
                           bottomBar.content(),
                           label("Perspective",
                                 toolui::rect(0.018f, 0.080f, 0.260f, 0.840f),
                                 ToolTheme::mutedText,
                                 ToolTheme::smallTextScale));
            cameraMode.build(context,
                             bottomBar.content(),
                             label("Camera",
                                   toolui::rect(0.760f, 0.080f, 0.220f, 0.840f),
                                   ToolTheme::mutedText,
                                   ToolTheme::smallTextScale,
                                   UiHorizontalAlign::Right));
            evalLabel.build(context,
                            evalChip.content(),
                            label("World Render Target",
                                  toolui::rect(0.055f, 0.080f, 0.890f, 0.840f),
                                  ToolTheme::statusText,
                                  ToolTheme::smallTextScale));
            debugLabel.build(context,
                             debugChip.content(),
                             label("Debug Draw Off",
                                   toolui::rect(0.055f, 0.080f, 0.890f, 0.840f),
                                   ToolTheme::statusText,
                                   ToolTheme::smallTextScale,
                                   UiHorizontalAlign::Right));
        }

        void update(gts::ui::UiWidgetContext& context,
                    const ToolShellView& view,
                    const UiLayoutSpec& layout,
                    bool visible) override
        {
            updateRoot(context, layout, visible);
            scene.setText(context, view.activeSceneName.empty()
                                       ? std::string("No scene")
                                       : "Scene: " + toolui::compact(view.activeSceneName, 42));
            title.setVisible(context, visible);
            scene.setVisible(context, visible);
            mode.setVisible(context, visible);
            viewMode.setVisible(context, visible);
            cameraMode.setVisible(context, visible);
            topBar.setVisible(context, visible);
            bottomBar.setVisible(context, visible);
            evalChip.setVisible(context, visible);
            debugChip.setVisible(context, visible);
            evalLabel.setVisible(context, visible);
            debugLabel.setVisible(context, visible);
        }

        void onEvent(gts::ui::UiWidgetContext&, UiEvent&, ToolCommandQueue&, const ToolShellView&) override {}

        void destroy(gts::ui::UiWidgetContext& context) override
        {
            title.destroy(context);
            scene.destroy(context);
            mode.destroy(context);
            viewMode.destroy(context);
            cameraMode.destroy(context);
            evalLabel.destroy(context);
            debugLabel.destroy(context);
            debugChip.destroy(context);
            evalChip.destroy(context);
            bottomBar.destroy(context);
            topBar.destroy(context);
            destroyRoot(context);
        }

    private:
        gts::ui::UiPanelWidget topBar;
        gts::ui::UiPanelWidget bottomBar;
        gts::ui::UiLabelWidget title;
        gts::ui::UiLabelWidget scene;
        gts::ui::UiLabelWidget mode;
        gts::ui::UiLabelWidget viewMode;
        gts::ui::UiLabelWidget cameraMode;
        gts::ui::UiPanelWidget evalChip;
        gts::ui::UiPanelWidget debugChip;
        gts::ui::UiLabelWidget evalLabel;
        gts::ui::UiLabelWidget debugLabel;
    };

    class SceneBrowserPane final : public ToolPaneBase
    {
    public:
        SceneBrowserPane()
            : ToolPaneBase({ToolPaneId::SceneBrowser, ToolDockArea::Left, "Scenes", true, false, 0})
        {
        }

        void build(gts::ui::UiWidgetContext& context,
                   UiHandle parent,
                   BitmapFont* font,
                   const UiLayoutSpec& layout) override
        {
            buildRoot(context, parent, font, layout, ToolTheme::paneBackground, true);

            ToolListSectionDesc desc;
            desc.title = "Scenes";
            desc.headerLayout = toolui::rect(0.050f, 0.030f, 0.430f, 0.040f);
            desc.countLayout = toolui::rect(0.500f, 0.032f, 0.450f, 0.034f);
            desc.stackLayout = toolui::rect(0.050f, 0.092f, 0.900f, 0.500f);
            desc.previousLayout = toolui::rect(0.050f, 0.620f, 0.430f, 0.050f);
            desc.nextLayout = toolui::rect(0.520f, 0.620f, 0.430f, 0.050f);
            desc.rowHeight = 0.043f;
            desc.rowGap = 0.006f;
            desc.headerScale = ToolTheme::headerTextScale;
            desc.rowTextScale = ToolTheme::bodyTextScale;
            desc.showCount = true;
            desc.showPager = true;
            scenes.build(context, content(), font, desc);
        }

        void update(gts::ui::UiWidgetContext& context,
                    const ToolShellView& view,
                    const UiLayoutSpec& layout,
                    bool visible) override
        {
            updateRoot(context, layout, visible);
            std::vector<ToolSelectableRowData> rows;
            rows.reserve(view.scenes.size());
            for (const ToolSceneRow& scene : view.scenes)
            {
                ToolSelectableRowData row;
                row.badge = "SC";
                row.primary = scene.label;
                row.enabled = !scene.active;
                row.selected = scene.active;
                rows.push_back(std::move(row));
            }
            const bool hasPrevious = visible && view.sceneOffset > 0;
            const bool hasNext = visible && view.sceneOffset + view.scenes.size() < view.sceneTotal;
            scenes.sync(context,
                        visible,
                        rows,
                        pageText(view.sceneOffset, view.scenes.size(), view.sceneTotal),
                        hasPrevious,
                        hasNext);
        }

        void onEvent(gts::ui::UiWidgetContext& context,
                     UiEvent& event,
                     ToolCommandQueue& commands,
                     const ToolShellView& view) override
        {
            scenes.onEvent(context, event);

            if (scenes.consumePreviousPressed())
                commands.push({ToolCommandType::ScenePagePrevious});
            if (scenes.consumeNextPressed())
                commands.push({ToolCommandType::ScenePageNext});

            size_t rowIndex = 0;
            if (scenes.consumeRowPressed(rowIndex) && rowIndex < view.scenes.size())
            {
                ToolCommand command;
                command.type = ToolCommandType::LoadScene;
                command.value = view.scenes[rowIndex].id;
                commands.push(std::move(command));
            }
        }

        void destroy(gts::ui::UiWidgetContext& context) override
        {
            scenes.destroy(context);
            destroyRoot(context);
        }

    private:
        static constexpr size_t MaxRows = 10;

        ToolListSection<MaxRows> scenes;
    };

    class EffectHierarchyPane final : public ToolPaneBase
    {
    public:
        EffectHierarchyPane()
            : ToolPaneBase({ToolPaneId::EffectHierarchy,
                            ToolDockArea::Left,
                            "Effect Hierarchy",
                            false,
                            true,
                            0,
                            0.760f})
        {
        }

        void build(gts::ui::UiWidgetContext& context,
                   UiHandle parent,
                   BitmapFont* font,
                   const UiLayoutSpec& layout) override
        {
            buildRoot(context, parent, font, layout, ToolTheme::paneBackground, true);

            ToolListSectionDesc effectDesc;
            effectDesc.title = "Particle Assets";
            effectDesc.headerLayout = toolui::rect(0.050f, 0.024f, 0.560f, 0.040f);
            effectDesc.countLayout = toolui::rect(0.620f, 0.028f, 0.330f, 0.034f);
            effectDesc.stackLayout = toolui::rect(0.050f, 0.086f, 0.900f, 0.290f);
            effectDesc.previousLayout = toolui::rect(0.050f, 0.395f, 0.430f, 0.044f);
            effectDesc.nextLayout = toolui::rect(0.520f, 0.395f, 0.430f, 0.044f);
            effectDesc.rowHeight = 0.038f;
            effectDesc.rowGap = 0.005f;
            effectDesc.headerScale = ToolTheme::headerTextScale;
            effectDesc.rowTextScale = ToolTheme::bodyTextScale;
            effectDesc.showCount = true;
            effectDesc.showPager = true;
            effects.build(context, content(), font, effectDesc);

            ToolListSectionDesc emitterDesc;
            emitterDesc.title = "Emitters";
            emitterDesc.headerLayout = toolui::rect(0.050f, 0.485f, 0.900f, 0.034f);
            emitterDesc.stackLayout = toolui::rect(0.050f, 0.530f, 0.900f, 0.220f);
            emitterDesc.rowHeight = 0.036f;
            emitterDesc.rowGap = 0.005f;
            emitterDesc.headerScale = ToolTheme::smallTextScale;
            emitterDesc.rowTextScale = ToolTheme::bodyTextScale;
            emitters.build(context, content(), font, emitterDesc);

            ToolListSectionDesc moduleDesc;
            moduleDesc.title = "Modules";
            moduleDesc.headerLayout = toolui::rect(0.050f, 0.780f, 0.900f, 0.034f);
            moduleDesc.stackLayout = toolui::rect(0.050f, 0.820f, 0.900f, 0.170f);
            moduleDesc.rowHeight = 0.033f;
            moduleDesc.rowGap = 0.004f;
            moduleDesc.headerScale = ToolTheme::smallTextScale;
            moduleDesc.rowTextScale = ToolTheme::bodyTextScale;
            modules.build(context, content(), font, moduleDesc);
        }

        void update(gts::ui::UiWidgetContext& context,
                    const ToolShellView& view,
                    const UiLayoutSpec& layout,
                    bool visible) override
        {
            updateRoot(context, layout, visible);
            std::vector<ToolSelectableRowData> effectRows;
            effectRows.reserve(view.effects.size());
            for (const ToolEffectRow& effect : view.effects)
            {
                ToolSelectableRowData row;
                row.badge = "FX";
                row.primary = effect.label;
                row.selected = effect.active;
                row.hasChildren = true;
                row.expanded = effect.active;
                effectRows.push_back(std::move(row));
            }

            const bool hasPrevious = visible && view.effectOffset > 0;
            const bool hasNext = visible && view.effectOffset + view.effects.size() < view.effectTotal;
            effects.sync(context,
                         visible,
                         effectRows,
                         pageText(view.effectOffset, view.effects.size(), view.effectTotal),
                         hasPrevious,
                         hasNext);

            std::vector<ToolSelectableRowData> emitterRows;
            emitterRows.reserve(view.emitters.size());
            for (const ToolEmitterRow& emitter : view.emitters)
            {
                ToolSelectableRowData row;
                row.badge = "E";
                row.primary = emitter.label;
                row.secondary = emitter.summary;
                row.depth = 1;
                row.selected = emitter.active;
                row.warning = !emitter.enabled;
                row.hasChildren = true;
                row.expanded = emitter.active;
                emitterRows.push_back(std::move(row));
            }
            emitters.sync(context, visible, emitterRows);

            std::vector<ToolSelectableRowData> moduleRows;
            moduleRows.reserve(view.modules.size());
            for (const ToolModuleRow& module : view.modules)
            {
                ToolSelectableRowData row;
                row.badge = "M";
                row.primary = module.label;
                row.secondary = module.summary;
                row.depth = 2;
                row.selected = module.active;
                row.warning = !module.enabled;
                moduleRows.push_back(std::move(row));
            }
            modules.sync(context, visible, moduleRows);
        }

        void onEvent(gts::ui::UiWidgetContext& context,
                     UiEvent& event,
                     ToolCommandQueue& commands,
                     const ToolShellView& view) override
        {
            effects.onEvent(context, event);
            emitters.onEvent(context, event);
            modules.onEvent(context, event);

            if (effects.consumePreviousPressed())
                commands.push({ToolCommandType::EffectPagePrevious});
            if (effects.consumeNextPressed())
                commands.push({ToolCommandType::EffectPageNext});

            size_t rowIndex = 0;
            if (effects.consumeRowPressed(rowIndex) && rowIndex < view.effects.size())
            {
                ToolCommand command;
                command.type = ToolCommandType::OpenParticleEffect;
                command.value = view.effects[rowIndex].path;
                commands.push(std::move(command));
            }

            if (emitters.consumeRowPressed(rowIndex) && rowIndex < view.emitters.size())
            {
                ToolCommand command;
                command.type = ToolCommandType::SelectEmitter;
                command.index = view.emitters[rowIndex].index;
                commands.push(command);
            }

            if (modules.consumeRowPressed(rowIndex) && rowIndex < view.modules.size())
            {
                ToolCommand command;
                command.type = ToolCommandType::SelectModule;
                command.index = view.modules[rowIndex].index;
                commands.push(command);
            }
        }

        void destroy(gts::ui::UiWidgetContext& context) override
        {
            effects.destroy(context);
            emitters.destroy(context);
            modules.destroy(context);
            destroyRoot(context);
        }

    private:
        static constexpr size_t MaxEffectRows = 6;
        static constexpr size_t MaxEmitterRows = 5;
        static constexpr size_t MaxModuleRows = 4;

        ToolListSection<MaxEffectRows> effects;
        ToolListSection<MaxEmitterRows> emitters;
        ToolListSection<MaxModuleRows> modules;
    };

    class EmitterDetailsPane final : public ToolPaneBase
    {
    public:
        EmitterDetailsPane()
            : ToolPaneBase({ToolPaneId::EmitterDetails,
                            ToolDockArea::Left,
                            "Emitter Details",
                            false,
                            true,
                            1,
                            0.240f})
        {
        }

        void build(gts::ui::UiWidgetContext& context,
                   UiHandle parent,
                   BitmapFont* font,
                   const UiLayoutSpec& layout) override
        {
            buildRoot(context, parent, font, layout, ToolTheme::paneBackground, true);

            ToolInspectorSectionDesc detailsDesc;
            detailsDesc.title = "Emitter Details";
            detailsDesc.headerLayout = toolui::rect(0.050f, 0.060f, 0.900f, 0.115f);
            detailsDesc.stackLayout = toolui::rect(0.050f, 0.210f, 0.900f, 0.245f);
            detailsDesc.lineHeight = ToolTheme::compactRowHeight;
            detailsDesc.lineGap = DefaultEditorTheme.spacing.xs;
            details.build(context, content(), font, detailsDesc);
        }

        void update(gts::ui::UiWidgetContext& context,
                    const ToolShellView& view,
                    const UiLayoutSpec& layout,
                    bool visible) override
        {
            updateRoot(context, layout, visible);

            const bool hasSelection = hasSelectedEmitter(view) || hasSelectedModule(view);
            details.sync(context,
                         visible && hasSelection,
                         "Emitter Details",
                         {selectedEmitterText(view), selectedModuleText(view)});
        }

        void onEvent(gts::ui::UiWidgetContext& context,
                     UiEvent& event,
                     ToolCommandQueue& commands,
                     const ToolShellView&) override
        {
        }

        void destroy(gts::ui::UiWidgetContext& context) override
        {
            details.destroy(context);
            destroyRoot(context);
        }

    private:
        static std::string selectedEmitterText(const ToolShellView& view)
        {
            const auto it = std::find_if(view.emitters.begin(),
                                         view.emitters.end(),
                                         [](const ToolEmitterRow& row)
                                         {
                                             return row.active;
                                         });
            return it == view.emitters.end() ? "No emitter selected" : "Emitter: " + it->label;
        }

        static std::string selectedModuleText(const ToolShellView& view)
        {
            const auto it = std::find_if(view.modules.begin(),
                                         view.modules.end(),
                                         [](const ToolModuleRow& row)
                                         {
                                             return row.active;
                                         });
            return it == view.modules.end() ? "No module selected" : "Module: " + it->label;
        }

        static bool hasSelectedEmitter(const ToolShellView& view)
        {
            return std::any_of(view.emitters.begin(),
                               view.emitters.end(),
                               [](const ToolEmitterRow& row)
                               {
                                   return row.active;
                               });
        }

        static bool hasSelectedModule(const ToolShellView& view)
        {
            return std::any_of(view.modules.begin(),
                               view.modules.end(),
                               [](const ToolModuleRow& row)
                               {
                                   return row.active;
                               });
        }

        ToolInspectorSection<2> details;
    };

    class ParticlePreviewViewportPane final : public ToolPaneBase
    {
    public:
        ParticlePreviewViewportPane()
            : ToolPaneBase({ToolPaneId::ParticlePreviewViewport,
                            ToolDockArea::Right,
                            "Particle Preview",
                            false,
                            true,
                            0,
                            0.340f})
        {
        }

        UiHandle previewImageHandle() const override { return previewImage.root(); }

        void build(gts::ui::UiWidgetContext& context,
                   UiHandle parent,
                   BitmapFont* font,
                   const UiLayoutSpec& layout) override
        {
            buildRoot(context, parent, font, layout, ToolTheme::paneBackground, true);
            buildPanel(context,
                       headerPanel,
                       content(),
                       toolui::rect(0.035f, 0.035f, 0.930f, 0.145f),
                       ToolTheme::headerBackground);
            title.build(context,
                        headerPanel.content(),
                        label("No Effect",
                              toolui::rect(0.024f, 0.060f, 0.920f, 0.450f),
                              ToolTheme::text,
                              ToolTheme::headerTextScale));
            path.build(context,
                       headerPanel.content(),
                       label("Select a particle asset",
                             toolui::rect(0.024f, 0.500f, 0.920f, 0.420f),
                             ToolTheme::mutedText,
                             ToolTheme::smallTextScale));

            gts::ui::UiPanelDesc previewDesc;
            previewDesc.layout = toolui::rect(0.050f, 0.205f, 0.900f, 0.480f);
            previewDesc.styleClass.clear();
            previewDesc.enabled = true;
            previewDesc.interactable = false;
            previewPanel.build(context, content(), previewDesc);
            toolui::setRectPayload(context.ui, context.surface, previewPanel.root(), ToolTheme::panelInset);

            previewImage.build(context, previewPanel.content(), image(toolui::rect(0.018f, 0.025f, 0.964f, 0.950f)));
            placeholder.build(context,
                              previewPanel.content(),
                              label("No preview",
                                    toolui::rect(0.050f, 0.420f, 0.900f, 0.160f),
                                    ToolTheme::mutedText,
                                    ToolTheme::smallTextScale,
                                    UiHorizontalAlign::Center));

            buildPanel(context,
                       statusStrip,
                       content(),
                       toolui::rect(0.050f, 0.705f, 0.900f, 0.060f),
                       ToolTheme::viewportOverlay);
            previewStatus.build(context,
                                statusStrip.content(),
                                label("Preview idle",
                                      toolui::rect(0.035f, 0.100f, 0.520f, 0.800f),
                                      ToolTheme::statusText,
                                      ToolTheme::smallTextScale));
            previewTarget.build(context,
                                statusStrip.content(),
                                label("Separate render target",
                                      toolui::rect(0.515f, 0.100f, 0.450f, 0.800f),
                                      ToolTheme::mutedText,
                                      ToolTheme::smallTextScale,
                                      UiHorizontalAlign::Right));

            buildPanel(context,
                       actionBar,
                       content(),
                       toolui::rect(0.050f, 0.790f, 0.900f, 0.105f),
                       ToolTheme::headerBackground);
            std::array<ToolToolbarButtonSlot, 4> slots;
            slots[0] = {"Save", toolui::rect(0.015f, 0.140f, 0.220f, 0.720f)};
            slots[1] = {"Reload", toolui::rect(0.255f, 0.140f, 0.220f, 0.720f)};
            slots[2] = {"Pause", toolui::rect(0.495f, 0.140f, 0.220f, 0.720f)};
            slots[3] = {"Restart", toolui::rect(0.735f, 0.140f, 0.250f, 0.720f)};
            actions.build(context, actionBar.content(), font, slots, ToolTheme::buttonTextScale);
        }

        void update(gts::ui::UiWidgetContext& context,
                    const ToolShellView& view,
                    const UiLayoutSpec& layout,
                    bool visible) override
        {
            updateRoot(context, layout, visible);
            const bool dominantPreview = view.activeWorkspace == ToolWorkspace::Particles;
            context.ui.setLayout(context.surface,
                                 headerPanel.root(),
                                 dominantPreview
                                     ? toolui::rect(0.024f, 0.026f, 0.952f, 0.110f)
                                     : toolui::rect(0.035f, 0.035f, 0.930f, 0.145f));
            context.ui.setLayout(context.surface,
                                 previewPanel.root(),
                                 dominantPreview
                                     ? toolui::rect(0.024f, 0.160f, 0.952f, 0.690f)
                                     : toolui::rect(0.050f, 0.205f, 0.900f, 0.480f));
            context.ui.setLayout(context.surface,
                                 statusStrip.root(),
                                 dominantPreview
                                     ? toolui::rect(0.024f, 0.875f, 0.465f, 0.060f)
                                     : toolui::rect(0.050f, 0.705f, 0.900f, 0.060f));
            context.ui.setLayout(context.surface,
                                 actionBar.root(),
                                 dominantPreview
                                     ? toolui::rect(0.510f, 0.875f, 0.466f, 0.060f)
                                     : toolui::rect(0.050f, 0.790f, 0.900f, 0.105f));

            title.setText(context, view.particleTitle + (view.particleDirty ? " *" : ""));
            path.setText(context,
                         view.particlePath.empty()
                             ? std::string("Select a particle asset")
                             : toolui::compact(view.particlePath, 62));

            const bool hasPreview = visible && view.previewTexture != 0;
            previewImage.setVisible(context, hasPreview);
            placeholder.setVisible(context, visible && !hasPreview);
            UiImageData imageData;
            imageData.textureID = view.previewTexture;
            imageData.tint = {1.0f, 1.0f, 1.0f, hasPreview ? 1.0f : 0.0f};
            imageData.imageAspect = 1.0f;
            context.ui.setPayload(context.surface, previewImage.root(), imageData);

            headerPanel.setVisible(context, visible);
            previewPanel.setVisible(context, visible);
            statusStrip.setVisible(context, visible);
            previewStatus.setVisible(context, visible);
            previewTarget.setVisible(context, visible);
            actionBar.setVisible(context, visible);
            previewStatus.setText(context,
                                  view.particleLoaded
                                      ? (view.particlePlaying ? "Preview playing" : "Preview paused")
                                      : "Preview idle");

            actions.sync(context, 0, visible, "Save", view.particleLoaded, false);
            actions.sync(context, 1, visible, "Reload", view.particleLoaded, false);
            actions.sync(context, 2, visible, view.particlePlaying ? "Pause" : "Play", view.particleLoaded, false);
            actions.sync(context, 3, visible, "Restart", view.particleLoaded, false);
        }

        void onEvent(gts::ui::UiWidgetContext& context,
                     UiEvent& event,
                     ToolCommandQueue& commands,
                     const ToolShellView&) override
        {
            actions.onEvent(context, event);

            if (actions.consumePressed(0))
                commands.push({ToolCommandType::SaveParticleEffect});
            if (actions.consumePressed(1))
                commands.push({ToolCommandType::ReloadParticleEffect});
            if (actions.consumePressed(2))
                commands.push({ToolCommandType::ToggleParticlePlayback});
            if (actions.consumePressed(3))
                commands.push({ToolCommandType::RestartParticlePreview});
        }

        void destroy(gts::ui::UiWidgetContext& context) override
        {
            title.destroy(context);
            path.destroy(context);
            placeholder.destroy(context);
            previewImage.destroy(context);
            previewPanel.destroy(context);
            previewTarget.destroy(context);
            previewStatus.destroy(context);
            statusStrip.destroy(context);
            actions.destroy(context);
            actionBar.destroy(context);
            headerPanel.destroy(context);
            destroyRoot(context);
        }

    private:
        gts::ui::UiPanelWidget headerPanel;
        gts::ui::UiLabelWidget title;
        gts::ui::UiLabelWidget path;
        gts::ui::UiPanelWidget previewPanel;
        gts::ui::UiImageWidget previewImage;
        gts::ui::UiLabelWidget placeholder;
        gts::ui::UiPanelWidget statusStrip;
        gts::ui::UiLabelWidget previewStatus;
        gts::ui::UiLabelWidget previewTarget;
        gts::ui::UiPanelWidget actionBar;
        ToolToolbarRow<4> actions;
    };

    class PropertyInspectorPane final : public ToolPaneBase
    {
    public:
        PropertyInspectorPane()
            : ToolPaneBase({ToolPaneId::PropertyInspector,
                            ToolDockArea::Right,
                            "Inspector",
                            true,
                            true,
                            1,
                            0.420f,
                            0.640f,
                            0.420f})
        {
        }

        void build(gts::ui::UiWidgetContext& context,
                   UiHandle parent,
                   BitmapFont* font,
                   const UiLayoutSpec& layout) override
        {
            buildRoot(context, parent, font, layout, ToolTheme::paneBackground, true);
            inspector.build(context, content(), font, toolui::rect(0.050f, 0.050f, 0.900f, 0.900f));
        }

        void update(gts::ui::UiWidgetContext& context,
                    const ToolShellView& view,
                    const UiLayoutSpec& layout,
                    bool visible) override
        {
            updateRoot(context, layout, visible);
            inspector.sync(context, visible, view.propertySections);
        }

        void onEvent(gts::ui::UiWidgetContext& context,
                     UiEvent& event,
                     ToolCommandQueue& commands,
                     const ToolShellView&) override
        {
            inspector.onEvent(context, event, commands);
        }

        void destroy(gts::ui::UiWidgetContext& context) override
        {
            inspector.destroy(context);
            destroyRoot(context);
        }

    private:
        ToolPropertyInspector<5, 18> inspector;
    };

    class CurveTimelinePane final : public ToolPaneBase
    {
    public:
        CurveTimelinePane()
            : ToolPaneBase({ToolPaneId::CurveTimeline, ToolDockArea::Bottom, "Curves", true, true, 0})
        {
        }

        void build(gts::ui::UiWidgetContext& context,
                   UiHandle parent,
                   BitmapFont* font,
                   const UiLayoutSpec& layout) override
        {
            buildRoot(context, parent, font, layout, ToolTheme::paneBackground, true);
            buildPanel(context,
                       curveListPanel,
                       content(),
                       toolui::rect(0.018f, 0.180f, 0.215f, 0.725f),
                       ToolTheme::panelInset);
            buildPanel(context,
                       timelinePanel,
                       content(),
                       toolui::rect(0.250f, 0.180f, 0.732f, 0.725f),
                       ToolTheme::panelInset);
            header.build(context,
                         content(),
                         label("Curves / Timeline",
                               toolui::rect(0.025f, 0.040f, 0.260f, 0.110f),
                               ToolTheme::text,
                               ToolTheme::smallTextScale));
            metadata.build(context,
                           content(),
                           label("No curve parameter selected",
                                 toolui::rect(0.305f, 0.040f, 0.440f, 0.110f),
                                 ToolTheme::mutedText,
                                 ToolTheme::smallTextScale));
            buildPanel(context,
                       tabStrip,
                       content(),
                       toolui::rect(0.765f, 0.036f, 0.210f, 0.120f),
                       ToolTheme::toolbarGroupBackground);
            curvesTab.build(context,
                            tabStrip.content(),
                            label("Curves",
                                  toolui::rect(0.040f, 0.080f, 0.300f, 0.840f),
                                  ToolTheme::text,
                                  ToolTheme::smallTextScale));
            timelineTab.build(context,
                              tabStrip.content(),
                              label("Timeline",
                                    toolui::rect(0.355f, 0.080f, 0.345f, 0.840f),
                                    ToolTheme::mutedText,
                                    ToolTheme::smallTextScale));
            graphTab.build(context,
                           tabStrip.content(),
                           label("Graph",
                                 toolui::rect(0.720f, 0.080f, 0.240f, 0.840f),
                                 ToolTheme::mutedText,
                                 ToolTheme::smallTextScale,
                                 UiHorizontalAlign::Right));
            const std::array<std::pair<const char*, UiColor>, 5> curves{{
                {"Spawn.Rate", ToolTheme::curveBlue},
                {"Lifetime", ToolTheme::curveGreen},
                {"Size", ToolTheme::curveYellow},
                {"Velocity", ToolTheme::curveViolet},
                {"Alpha", ToolTheme::mutedText}
            }};
            for (size_t i = 0; i < curveRows.size(); ++i)
            {
                const float y = 0.070f + static_cast<float>(i) * 0.160f;
                buildPanel(context,
                           curveRows[i],
                           curveListPanel.content(),
                           toolui::rect(0.045f, y, 0.090f, 0.060f),
                           curves[i].second);
                curveLabels[i].build(context,
                                     curveListPanel.content(),
                                     label(curves[i].first,
                                           toolui::rect(0.160f, y - 0.020f, 0.780f, 0.100f),
                                           ToolTheme::mutedText,
                                           ToolTheme::smallTextScale));
            }
            for (size_t i = 0; i < timeTicks.size(); ++i)
            {
                const float x = 0.070f + static_cast<float>(i) * 0.215f;
                buildPanel(context,
                           timeTicks[i],
                           timelinePanel.content(),
                           toolui::rect(x, 0.130f, 0.0025f, 0.760f),
                           ToolTheme::borderSubtle);
            }
            const std::array<UiColor, 4> lineColors{
                ToolTheme::curveBlue,
                ToolTheme::curveGreen,
                ToolTheme::curveYellow,
                ToolTheme::curveViolet
            };
            for (size_t i = 0; i < trackLines.size(); ++i)
            {
                const float y = 0.300f + static_cast<float>(i) * 0.130f;
                buildPanel(context,
                           trackLines[i],
                           timelinePanel.content(),
                           toolui::rect(0.070f, y, 0.865f, 0.010f),
                           lineColors[i]);
            }
            buildPanel(context,
                       playhead,
                       timelinePanel.content(),
                       toolui::rect(0.560f, 0.090f, 0.004f, 0.820f),
                       ToolTheme::accent);
            buildPanel(context,
                       playheadBadge,
                       timelinePanel.content(),
                       toolui::rect(0.505f, 0.870f, 0.118f, 0.075f),
                       ToolTheme::viewportOverlay);
            playheadLabel.build(context,
                                playheadBadge.content(),
                                label("2.3s",
                                      toolui::rect(0.060f, 0.080f, 0.880f, 0.840f),
                                      ToolTheme::statusText,
                                      ToolTheme::smallTextScale,
                                      UiHorizontalAlign::Center));
            lane.build(context,
                       timelinePanel.content(),
                       label("0s        1s        2s        3s        4s",
                             toolui::rect(0.045f, 0.030f, 0.900f, 0.090f),
                             ToolTheme::mutedText,
                             ToolTheme::smallTextScale,
                             UiHorizontalAlign::Right));
        }

        void update(gts::ui::UiWidgetContext& context,
                    const ToolShellView& view,
                    const UiLayoutSpec& layout,
                    bool visible) override
        {
            updateRoot(context, layout, visible);
            curveListPanel.setVisible(context, visible);
            timelinePanel.setVisible(context, visible);
            tabStrip.setVisible(context, visible);
            curvesTab.setVisible(context, visible);
            timelineTab.setVisible(context, visible);
            graphTab.setVisible(context, visible);
            for (auto& row : curveRows)
                row.setVisible(context, visible);
            for (auto& label : curveLabels)
                label.setVisible(context, visible);
            for (auto& tick : timeTicks)
                tick.setVisible(context, visible);
            for (auto& line : trackLines)
                line.setVisible(context, visible);
            playhead.setVisible(context, visible);
            playheadBadge.setVisible(context, visible);
            playheadLabel.setVisible(context, visible);
            header.setVisible(context, visible);
            metadata.setVisible(context, visible);
            lane.setVisible(context, visible);
            metadata.setText(context,
                             view.activeWorkspace == ToolWorkspace::Particles
                                 ? "Focused particle curves will appear here"
                                 : "World timeline placeholder");
        }

        void onEvent(gts::ui::UiWidgetContext&, UiEvent&, ToolCommandQueue&, const ToolShellView&) override {}

        void destroy(gts::ui::UiWidgetContext& context) override
        {
            header.destroy(context);
            metadata.destroy(context);
            lane.destroy(context);
            playheadLabel.destroy(context);
            playheadBadge.destroy(context);
            graphTab.destroy(context);
            timelineTab.destroy(context);
            curvesTab.destroy(context);
            tabStrip.destroy(context);
            for (auto& line : trackLines)
                line.destroy(context);
            for (auto& tick : timeTicks)
                tick.destroy(context);
            for (auto& label : curveLabels)
                label.destroy(context);
            for (auto& row : curveRows)
                row.destroy(context);
            playhead.destroy(context);
            timelinePanel.destroy(context);
            curveListPanel.destroy(context);
            destroyRoot(context);
        }

    private:
        gts::ui::UiPanelWidget curveListPanel;
        gts::ui::UiPanelWidget timelinePanel;
        gts::ui::UiLabelWidget header;
        gts::ui::UiLabelWidget metadata;
        gts::ui::UiLabelWidget lane;
        gts::ui::UiPanelWidget tabStrip;
        gts::ui::UiLabelWidget curvesTab;
        gts::ui::UiLabelWidget timelineTab;
        gts::ui::UiLabelWidget graphTab;
        std::array<gts::ui::UiPanelWidget, 5> curveRows;
        std::array<gts::ui::UiLabelWidget, 5> curveLabels;
        std::array<gts::ui::UiPanelWidget, 5> timeTicks;
        std::array<gts::ui::UiPanelWidget, 4> trackLines;
        gts::ui::UiPanelWidget playhead;
        gts::ui::UiPanelWidget playheadBadge;
        gts::ui::UiLabelWidget playheadLabel;
    };

    class DiagnosticsPane final : public ToolPaneBase
    {
    public:
        DiagnosticsPane()
            : ToolPaneBase({ToolPaneId::Diagnostics, ToolDockArea::Right, "Diagnostics", true, true, 2})
        {
        }

        void build(gts::ui::UiWidgetContext& context,
                   UiHandle parent,
                   BitmapFont* font,
                   const UiLayoutSpec& layout) override
        {
            buildRoot(context, parent, font, layout, ToolTheme::paneBackground, true);

            ToolInspectorSectionDesc desc;
            desc.title = "Diagnostics";
            desc.headerLayout = toolui::rect(0.050f, 0.065f, 0.900f, 0.100f);
            desc.stackLayout = toolui::rect(0.050f, 0.210f, 0.900f, 0.700f);
            desc.lineHeight = 0.105f;
            desc.lineGap = 0.020f;
            desc.titleScale = ToolTheme::smallTextScale;
            diagnostics.build(context, content(), font, desc);
        }

        void update(gts::ui::UiWidgetContext& context,
                    const ToolShellView& view,
                    const UiLayoutSpec& layout,
                    bool visible) override
        {
            updateRoot(context, layout, visible);
            diagnostics.sync(context, visible, "Diagnostics", view.diagnostics);
        }

        void onEvent(gts::ui::UiWidgetContext&, UiEvent&, ToolCommandQueue&, const ToolShellView&) override {}

        void destroy(gts::ui::UiWidgetContext& context) override
        {
            diagnostics.destroy(context);
            destroyRoot(context);
        }

    private:
        static constexpr size_t MaxLines = 5;

        ToolInspectorSection<MaxLines> diagnostics;
    };

    class StatusBarPane final : public ToolPaneBase
    {
    public:
        StatusBarPane()
            : ToolPaneBase({ToolPaneId::StatusBar, ToolDockArea::StatusBar, "Status", true, true, 0})
        {
        }

        void build(gts::ui::UiWidgetContext& context,
                   UiHandle parent,
                   BitmapFont* font,
                   const UiLayoutSpec& layout) override
        {
            buildRoot(context, parent, font, layout, transparent(), false);
            status.build(context,
                         content(),
                         label("F6 TO HIDE",
                               toolui::rect(0.014f, 0.120f, 0.960f, 0.760f),
                               ToolTheme::statusText,
                               ToolTheme::smallTextScale));
        }

        void update(gts::ui::UiWidgetContext& context,
                    const ToolShellView& view,
                    const UiLayoutSpec& layout,
                    bool visible) override
        {
            updateRoot(context, layout, visible);
            status.setText(context, view.status.empty() ? "F6 TO HIDE" : view.status);
        }

        void onEvent(gts::ui::UiWidgetContext&, UiEvent&, ToolCommandQueue&, const ToolShellView&) override {}

        void destroy(gts::ui::UiWidgetContext& context) override
        {
            status.destroy(context);
            destroyRoot(context);
        }

    private:
        gts::ui::UiLabelWidget status;
    };
} // namespace gts::tools
