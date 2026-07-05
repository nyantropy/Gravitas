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
                        label("GRAVITAS TOOLS",
                              toolui::rect(0.014f, 0.100f, 0.150f, 0.800f),
                              ToolTheme::text,
                              ToolTheme::titleTextScale));
            menuItems.build(context,
                            content(),
                            label("FILE   VIEW   TOOLS",
                                  toolui::rect(0.178f, 0.120f, 0.260f, 0.760f),
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
                            0.360f,
                            0.0f,
                            0.0f,
                            0.0f,
                            0.185f})
        {
        }

        void build(gts::ui::UiWidgetContext& context,
                   UiHandle parent,
                   BitmapFont* font,
                   const UiLayoutSpec& layout) override
        {
            buildRoot(context, parent, font, layout, transparent(), false);

            std::array<ToolToolbarButtonSlot, 2> slots;
            slots[0] = {"WORLD", toolui::rect(0.000f, 0.140f, 0.300f, 0.720f)};
            slots[1] = {"PARTICLES", toolui::rect(0.320f, 0.140f, 0.400f, 0.720f)};
            tabs.build(context, content(), font, slots, ToolTheme::buttonTextScale);
        }

        void update(gts::ui::UiWidgetContext& context,
                    const ToolShellView& view,
                    const UiLayoutSpec& layout,
                    bool visible) override
        {
            updateRoot(context, layout, visible);
            tabs.sync(context, 0, visible, "WORLD", true, view.activeWorkspace == ToolWorkspace::World);
            tabs.sync(context, 1, visible, "PARTICLES", true, view.activeWorkspace == ToolWorkspace::Particles);
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
            viewportLabel.build(context,
                                content(),
                                label("VIEWPORT",
                                      toolui::rect(0.014f, 0.130f, 0.095f, 0.740f),
                                      ToolTheme::mutedText,
                                      ToolTheme::smallTextScale));
            sceneLabel.build(context,
                             content(),
                             label("Scene",
                                   toolui::rect(0.115f, 0.130f, 0.620f, 0.740f),
                                   ToolTheme::text,
                                   ToolTheme::smallTextScale));
            workspaceLabel.build(context,
                                 content(),
                                 label("Workspace",
                                       toolui::rect(0.770f, 0.130f, 0.210f, 0.740f),
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
            sceneLabel.setText(context, "Scene: " + activeScene);
            workspaceLabel.setText(context,
                                   view.activeWorkspace == ToolWorkspace::Particles ? "Particles" : "World");
        }

        void onEvent(gts::ui::UiWidgetContext&, UiEvent&, ToolCommandQueue&, const ToolShellView&) override {}

        void destroy(gts::ui::UiWidgetContext& context) override
        {
            viewportLabel.destroy(context);
            sceneLabel.destroy(context);
            workspaceLabel.destroy(context);
            destroyRoot(context);
        }

    private:
        gts::ui::UiLabelWidget viewportLabel;
        gts::ui::UiLabelWidget sceneLabel;
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
            destroyRoot(context);
        }
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
            desc.stackLayout = toolui::rect(0.050f, 0.092f, 0.900f, 0.780f);
            desc.previousLayout = toolui::rect(0.050f, 0.910f, 0.430f, 0.050f);
            desc.nextLayout = toolui::rect(0.520f, 0.910f, 0.430f, 0.050f);
            desc.rowHeight = 0.056f;
            desc.rowGap = 0.008f;
            desc.headerScale = ToolTheme::headerTextScale;
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
                            0.660f})
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
            effectDesc.stackLayout = toolui::rect(0.050f, 0.086f, 0.900f, 0.292f);
            effectDesc.previousLayout = toolui::rect(0.050f, 0.395f, 0.430f, 0.044f);
            effectDesc.nextLayout = toolui::rect(0.520f, 0.395f, 0.430f, 0.044f);
            effectDesc.rowHeight = 0.044f;
            effectDesc.rowGap = 0.006f;
            effectDesc.headerScale = ToolTheme::headerTextScale;
            effectDesc.showCount = true;
            effectDesc.showPager = true;
            effects.build(context, content(), font, effectDesc);

            ToolListSectionDesc emitterDesc;
            emitterDesc.title = "Emitters";
            emitterDesc.headerLayout = toolui::rect(0.050f, 0.485f, 0.900f, 0.034f);
            emitterDesc.stackLayout = toolui::rect(0.050f, 0.530f, 0.900f, 0.230f);
            emitterDesc.rowHeight = 0.040f;
            emitterDesc.rowGap = 0.006f;
            emitterDesc.headerScale = ToolTheme::smallTextScale;
            emitters.build(context, content(), font, emitterDesc);

            ToolListSectionDesc moduleDesc;
            moduleDesc.title = "Modules";
            moduleDesc.headerLayout = toolui::rect(0.050f, 0.780f, 0.900f, 0.034f);
            moduleDesc.stackLayout = toolui::rect(0.050f, 0.824f, 0.900f, 0.160f);
            moduleDesc.rowHeight = 0.035f;
            moduleDesc.rowGap = 0.005f;
            moduleDesc.headerScale = ToolTheme::smallTextScale;
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
                row.primary = effect.label;
                row.selected = effect.active;
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
                row.primary = emitter.label;
                row.secondary = emitter.summary;
                row.selected = emitter.active;
                row.warning = !emitter.enabled;
                emitterRows.push_back(std::move(row));
            }
            emitters.sync(context, visible, emitterRows);

            std::vector<ToolSelectableRowData> moduleRows;
            moduleRows.reserve(view.modules.size());
            for (const ToolModuleRow& module : view.modules)
            {
                ToolSelectableRowData row;
                row.primary = module.label;
                row.secondary = module.summary;
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
            : ToolPaneBase({ToolPaneId::EmitterDetails, ToolDockArea::Left, "Emitter Details", false, true, 1})
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
            detailsDesc.headerLayout = toolui::rect(0.050f, 0.060f, 0.900f, 0.090f);
            detailsDesc.stackLayout = toolui::rect(0.050f, 0.190f, 0.900f, 0.200f);
            detailsDesc.lineHeight = 0.090f;
            detailsDesc.lineGap = 0.020f;
            details.build(context, content(), font, detailsDesc);
        }

        void update(gts::ui::UiWidgetContext& context,
                    const ToolShellView& view,
                    const UiLayoutSpec& layout,
                    bool visible) override
        {
            updateRoot(context, layout, visible);

            details.sync(context, visible, "Emitter Details", {selectedEmitterText(view), selectedModuleText(view)});
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
            title.build(context,
                        content(),
                        label("No Effect",
                              toolui::rect(0.050f, 0.050f, 0.900f, 0.070f),
                              ToolTheme::text,
                              ToolTheme::headerTextScale));
            path.build(context,
                       content(),
                       label("Select a particle asset",
                             toolui::rect(0.050f, 0.125f, 0.900f, 0.055f),
                             ToolTheme::mutedText,
                             ToolTheme::smallTextScale));

            gts::ui::UiPanelDesc previewDesc;
            previewDesc.layout = toolui::rect(0.050f, 0.210f, 0.900f, 0.500f);
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

            std::array<ToolToolbarButtonSlot, 4> slots;
            slots[0] = {"SAVE", toolui::rect(0.050f, 0.760f, 0.205f, 0.090f)};
            slots[1] = {"RELOAD", toolui::rect(0.275f, 0.760f, 0.205f, 0.090f)};
            slots[2] = {"PAUSE", toolui::rect(0.500f, 0.760f, 0.205f, 0.090f)};
            slots[3] = {"RESTART", toolui::rect(0.725f, 0.760f, 0.225f, 0.090f)};
            actions.build(context, content(), font, slots, ToolTheme::buttonTextScale);
        }

        void update(gts::ui::UiWidgetContext& context,
                    const ToolShellView& view,
                    const UiLayoutSpec& layout,
                    bool visible) override
        {
            updateRoot(context, layout, visible);
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

            actions.sync(context, 0, visible, "SAVE", view.particleLoaded, false);
            actions.sync(context, 1, visible, "RELOAD", view.particleLoaded, false);
            actions.sync(context, 2, visible, view.particlePlaying ? "PAUSE" : "PLAY", view.particleLoaded, false);
            actions.sync(context, 3, visible, "RESTART", view.particleLoaded, false);
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
            actions.destroy(context);
            destroyRoot(context);
        }

    private:
        gts::ui::UiLabelWidget title;
        gts::ui::UiLabelWidget path;
        gts::ui::UiPanelWidget previewPanel;
        gts::ui::UiImageWidget previewImage;
        gts::ui::UiLabelWidget placeholder;
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
            header.build(context,
                         content(),
                         label("Curves / Timeline",
                               toolui::rect(0.025f, 0.120f, 0.280f, 0.220f),
                               ToolTheme::text,
                               ToolTheme::smallTextScale));
            metadata.build(context,
                           content(),
                           label("No curve parameter selected",
                                 toolui::rect(0.025f, 0.400f, 0.440f, 0.220f),
                                 ToolTheme::mutedText,
                                 ToolTheme::smallTextScale));
            lane.build(context,
                       content(),
                       label("0s        1s        2s        3s        4s",
                             toolui::rect(0.440f, 0.230f, 0.530f, 0.300f),
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
            destroyRoot(context);
        }

    private:
        gts::ui::UiLabelWidget header;
        gts::ui::UiLabelWidget metadata;
        gts::ui::UiLabelWidget lane;
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
