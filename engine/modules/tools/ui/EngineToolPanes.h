#pragma once

#include <algorithm>
#include <array>
#include <string>
#include <utility>

#include "EngineToolUiHelpers.h"
#include "ToolPane.h"
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
            gts::ui::UiLabelDesc desc;
            desc.layout = layout;
            desc.text = text;
            desc.font = font;
            desc.styleClass.clear();
            desc.color = color;
            desc.scale = scale;
            desc.horizontalAlign = align;
            desc.verticalAlign = UiVerticalAlign::Middle;
            desc.wrapMode = UiTextWrapMode::Word;
            desc.maxLines = maxLines;
            return desc;
        }

        gts::ui::UiButtonDesc button(const std::string& text,
                                     const UiLayoutSpec& layout,
                                     float scale,
                                     UiHorizontalAlign align = UiHorizontalAlign::Center) const
        {
            gts::ui::UiButtonDesc desc;
            desc.layout = layout;
            desc.text = text;
            desc.font = font;
            desc.styleClass.clear();
            desc.labelStyleClass.clear();
            desc.textColor = ToolTheme::text;
            desc.textScale = scale;
            desc.horizontalAlign = align;
            desc.verticalAlign = UiVerticalAlign::Middle;
            desc.wrapMode = UiTextWrapMode::Word;
            desc.maxLines = 2;
            return desc;
        }

        static gts::ui::UiImageDesc image(const UiLayoutSpec& layout)
        {
            gts::ui::UiImageDesc desc;
            desc.layout = layout;
            desc.tint = {1.0f, 1.0f, 1.0f, 0.0f};
            desc.visible = false;
            return desc;
        }

        static UiLayoutSpec stackItem(float height)
        {
            UiLayoutSpec layout;
            layout.constraints.preferredHeight = {UiLayoutUnit::Normalized, height};
            return layout;
        }

        void syncButton(gts::ui::UiWidgetContext& context,
                        gts::ui::UiButtonWidget& widget,
                        bool visible,
                        const std::string& text,
                        bool enabled,
                        bool active)
        {
            widget.setVisible(context, visible);
            widget.setEnabled(context, visible && enabled);
            if (visible)
                widget.setText(context, text);
            paintButton(context, widget, active);
        }

        void paintButton(gts::ui::UiWidgetContext& context,
                         gts::ui::UiButtonWidget& widget,
                         bool active)
        {
            const UiNode* node = context.ui.findNode(context.surface, widget.root());
            if (node == nullptr)
                return;

            UiColor fill = active ? ToolTheme::buttonActive : ToolTheme::button;
            UiColor text = active ? ToolTheme::accent : ToolTheme::text;
            if (!node->state.enabled)
            {
                fill = ToolTheme::disabled;
                text = ToolTheme::disabledText;
            }
            else if (node->state.pressed)
            {
                fill = ToolTheme::buttonPressed;
            }
            else if (node->state.hovered)
            {
                fill = active ? ToolTheme::toggleHover : ToolTheme::buttonHover;
            }

            toolui::setRectPayload(context.ui, context.surface, widget.root(), fill);
            toolui::setTextColor(context.ui, context.surface, widget.label(), text);
        }

        static std::string pageText(size_t offset, size_t visible, size_t total)
        {
            if (total == 0)
                return "0";
            return std::to_string(offset + 1) + "-" + std::to_string(offset + visible) + " / " +
                std::to_string(total);
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
            : ToolPaneBase({ToolPaneId::MenuBar, ToolDockArea::Top, "Menu", true, true, 0})
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
            : ToolPaneBase({ToolPaneId::WorkspaceTabs, ToolDockArea::Top, "Workspaces", true, true, 1})
        {
        }

        void build(gts::ui::UiWidgetContext& context,
                   UiHandle parent,
                   BitmapFont* font,
                   const UiLayoutSpec& layout) override
        {
            buildRoot(context, parent, font, layout, transparent(), false);
            worldTab.build(context,
                           content(),
                           button("WORLD", toolui::rect(0.000f, 0.140f, 0.300f, 0.720f), ToolTheme::buttonTextScale));
            particlesTab.build(context,
                               content(),
                               button("PARTICLES",
                                      toolui::rect(0.320f, 0.140f, 0.400f, 0.720f),
                                      ToolTheme::buttonTextScale));
        }

        void update(gts::ui::UiWidgetContext& context,
                    const ToolShellView& view,
                    const UiLayoutSpec& layout,
                    bool visible) override
        {
            updateRoot(context, layout, visible);
            syncButton(context, worldTab, visible, "WORLD", true, view.activeWorkspace == ToolWorkspace::World);
            syncButton(context,
                       particlesTab,
                       visible,
                       "PARTICLES",
                       true,
                       view.activeWorkspace == ToolWorkspace::Particles);
        }

        void onEvent(gts::ui::UiWidgetContext& context,
                     UiEvent& event,
                     ToolCommandQueue& commands,
                     const ToolShellView&) override
        {
            worldTab.onEvent(context, event);
            particlesTab.onEvent(context, event);

            if (worldTab.consumePressed())
                commands.push({ToolCommandType::SetWorkspace, ToolWorkspace::World});
            if (particlesTab.consumePressed())
                commands.push({ToolCommandType::SetWorkspace, ToolWorkspace::Particles});
        }

        void destroy(gts::ui::UiWidgetContext& context) override
        {
            worldTab.destroy(context);
            particlesTab.destroy(context);
            destroyRoot(context);
        }

    private:
        gts::ui::UiButtonWidget worldTab;
        gts::ui::UiButtonWidget particlesTab;
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
            header.build(context,
                         content(),
                         label("Scenes",
                               toolui::rect(0.050f, 0.030f, 0.430f, 0.040f),
                               ToolTheme::text,
                               ToolTheme::headerTextScale));
            count.build(context,
                        content(),
                        label("0",
                              toolui::rect(0.500f, 0.032f, 0.450f, 0.034f),
                              ToolTheme::mutedText,
                              ToolTheme::smallTextScale,
                              UiHorizontalAlign::Right));

            gts::ui::UiStackDesc rowStackDesc;
            rowStackDesc.layout = toolui::rect(0.050f, 0.092f, 0.900f, 0.780f);
            rowStackDesc.axis = UiLayoutAxis::Vertical;
            rowStackDesc.gap = 0.008f;
            rowStack.build(context, content(), rowStackDesc);
            for (auto& row : rows)
            {
                row.build(context,
                          rowStack.root(),
                          button("--", stackItem(0.056f), ToolTheme::smallTextScale, UiHorizontalAlign::Left));
            }

            previous.build(context,
                           content(),
                           button("PREV", toolui::rect(0.050f, 0.910f, 0.430f, 0.050f), ToolTheme::buttonTextScale));
            next.build(context,
                       content(),
                       button("NEXT", toolui::rect(0.520f, 0.910f, 0.430f, 0.050f), ToolTheme::buttonTextScale));
        }

        void update(gts::ui::UiWidgetContext& context,
                    const ToolShellView& view,
                    const UiLayoutSpec& layout,
                    bool visible) override
        {
            updateRoot(context, layout, visible);
            count.setText(context, pageText(view.sceneOffset, view.scenes.size(), view.sceneTotal));
            for (size_t i = 0; i < rows.size(); ++i)
            {
                const bool rowVisible = visible && i < view.scenes.size();
                syncButton(context,
                           rows[i],
                           rowVisible,
                           rowVisible ? view.scenes[i].label : "--",
                           rowVisible && !view.scenes[i].active,
                           rowVisible && view.scenes[i].active);
            }
            const bool hasPrevious = visible && view.sceneOffset > 0;
            const bool hasNext = visible && view.sceneOffset + view.scenes.size() < view.sceneTotal;
            syncButton(context, previous, visible, "PREV", hasPrevious, false);
            syncButton(context, next, visible, "NEXT", hasNext, false);
        }

        void onEvent(gts::ui::UiWidgetContext& context,
                     UiEvent& event,
                     ToolCommandQueue& commands,
                     const ToolShellView& view) override
        {
            previous.onEvent(context, event);
            next.onEvent(context, event);
            for (auto& row : rows)
                row.onEvent(context, event);

            if (previous.consumePressed())
                commands.push({ToolCommandType::ScenePagePrevious});
            if (next.consumePressed())
                commands.push({ToolCommandType::ScenePageNext});

            for (size_t i = 0; i < rows.size(); ++i)
            {
                if (rows[i].consumePressed() && i < view.scenes.size())
                {
                    ToolCommand command;
                    command.type = ToolCommandType::LoadScene;
                    command.value = view.scenes[i].id;
                    commands.push(std::move(command));
                }
            }
        }

        void destroy(gts::ui::UiWidgetContext& context) override
        {
            header.destroy(context);
            count.destroy(context);
            for (auto& row : rows)
                row.destroy(context);
            rowStack.destroy(context);
            previous.destroy(context);
            next.destroy(context);
            destroyRoot(context);
        }

    private:
        static constexpr size_t MaxRows = 10;

        gts::ui::UiLabelWidget header;
        gts::ui::UiLabelWidget count;
        gts::ui::UiStackWidget rowStack;
        std::array<gts::ui::UiButtonWidget, MaxRows> rows;
        gts::ui::UiButtonWidget previous;
        gts::ui::UiButtonWidget next;
    };

    class EffectHierarchyPane final : public ToolPaneBase
    {
    public:
        EffectHierarchyPane()
            : ToolPaneBase({ToolPaneId::EffectHierarchy, ToolDockArea::Left, "Effect Hierarchy", false, true, 0})
        {
        }

        void build(gts::ui::UiWidgetContext& context,
                   UiHandle parent,
                   BitmapFont* font,
                   const UiLayoutSpec& layout) override
        {
            buildRoot(context, parent, font, layout, ToolTheme::paneBackground, true);
            effectHeader.build(context,
                               content(),
                               label("Particle Assets",
                                     toolui::rect(0.050f, 0.024f, 0.560f, 0.040f),
                                     ToolTheme::text,
                                     ToolTheme::headerTextScale));
            effectCount.build(context,
                              content(),
                              label("0",
                                    toolui::rect(0.620f, 0.028f, 0.330f, 0.034f),
                                    ToolTheme::mutedText,
                                    ToolTheme::smallTextScale,
                                    UiHorizontalAlign::Right));

            gts::ui::UiStackDesc effectStackDesc;
            effectStackDesc.layout = toolui::rect(0.050f, 0.086f, 0.900f, 0.292f);
            effectStackDesc.axis = UiLayoutAxis::Vertical;
            effectStackDesc.gap = 0.006f;
            effectStack.build(context, content(), effectStackDesc);
            for (auto& row : effectRows)
            {
                row.build(context,
                          effectStack.root(),
                          button("--", stackItem(0.044f), ToolTheme::smallTextScale, UiHorizontalAlign::Left));
            }

            effectPrevious.build(context,
                                 content(),
                                 button("PREV",
                                        toolui::rect(0.050f, 0.395f, 0.430f, 0.044f),
                                        ToolTheme::buttonTextScale));
            effectNext.build(context,
                             content(),
                             button("NEXT",
                                    toolui::rect(0.520f, 0.395f, 0.430f, 0.044f),
                                    ToolTheme::buttonTextScale));

            emitterHeader.build(context,
                                content(),
                                label("Emitters",
                                      toolui::rect(0.050f, 0.485f, 0.900f, 0.034f),
                                      ToolTheme::text,
                                      ToolTheme::smallTextScale));

            gts::ui::UiStackDesc emitterStackDesc;
            emitterStackDesc.layout = toolui::rect(0.050f, 0.530f, 0.900f, 0.230f);
            emitterStackDesc.axis = UiLayoutAxis::Vertical;
            emitterStackDesc.gap = 0.006f;
            emitterStack.build(context, content(), emitterStackDesc);
            for (auto& row : emitterRows)
            {
                row.build(context,
                          emitterStack.root(),
                          button("--", stackItem(0.040f), ToolTheme::smallTextScale, UiHorizontalAlign::Left));
            }

            moduleHeader.build(context,
                               content(),
                               label("Modules",
                                     toolui::rect(0.050f, 0.780f, 0.900f, 0.034f),
                                     ToolTheme::text,
                                     ToolTheme::smallTextScale));

            gts::ui::UiStackDesc moduleStackDesc;
            moduleStackDesc.layout = toolui::rect(0.050f, 0.824f, 0.900f, 0.160f);
            moduleStackDesc.axis = UiLayoutAxis::Vertical;
            moduleStackDesc.gap = 0.005f;
            moduleStack.build(context, content(), moduleStackDesc);
            for (auto& row : moduleRows)
            {
                row.build(context,
                          moduleStack.root(),
                          button("--", stackItem(0.035f), ToolTheme::smallTextScale, UiHorizontalAlign::Left));
            }
        }

        void update(gts::ui::UiWidgetContext& context,
                    const ToolShellView& view,
                    const UiLayoutSpec& layout,
                    bool visible) override
        {
            updateRoot(context, layout, visible);
            effectCount.setText(context, pageText(view.effectOffset, view.effects.size(), view.effectTotal));

            for (size_t i = 0; i < effectRows.size(); ++i)
            {
                const bool rowVisible = visible && i < view.effects.size();
                syncButton(context,
                           effectRows[i],
                           rowVisible,
                           rowVisible ? view.effects[i].label : "--",
                           rowVisible,
                           rowVisible && view.effects[i].active);
            }

            const bool hasPrevious = visible && view.effectOffset > 0;
            const bool hasNext = visible && view.effectOffset + view.effects.size() < view.effectTotal;
            syncButton(context, effectPrevious, visible, "PREV", hasPrevious, false);
            syncButton(context, effectNext, visible, "NEXT", hasNext, false);

            for (size_t i = 0; i < emitterRows.size(); ++i)
            {
                const bool rowVisible = visible && i < view.emitters.size();
                const std::string text = rowVisible
                    ? view.emitters[i].label + "  " + view.emitters[i].summary
                    : "--";
                syncButton(context,
                           emitterRows[i],
                           rowVisible,
                           text,
                           rowVisible,
                           rowVisible && view.emitters[i].active);
                if (rowVisible && !view.emitters[i].enabled)
                    toolui::setTextColor(context.ui, context.surface, emitterRows[i].label(), ToolTheme::warning);
            }

            for (size_t i = 0; i < moduleRows.size(); ++i)
            {
                const bool rowVisible = visible && i < view.modules.size();
                const std::string text = rowVisible
                    ? view.modules[i].label + "  " + view.modules[i].summary
                    : "--";
                syncButton(context,
                           moduleRows[i],
                           rowVisible,
                           text,
                           rowVisible,
                           rowVisible && view.modules[i].active);
                if (rowVisible && !view.modules[i].enabled)
                    toolui::setTextColor(context.ui, context.surface, moduleRows[i].label(), ToolTheme::warning);
            }
        }

        void onEvent(gts::ui::UiWidgetContext& context,
                     UiEvent& event,
                     ToolCommandQueue& commands,
                     const ToolShellView& view) override
        {
            effectPrevious.onEvent(context, event);
            effectNext.onEvent(context, event);
            for (auto& row : effectRows)
                row.onEvent(context, event);
            for (auto& row : emitterRows)
                row.onEvent(context, event);
            for (auto& row : moduleRows)
                row.onEvent(context, event);

            if (effectPrevious.consumePressed())
                commands.push({ToolCommandType::EffectPagePrevious});
            if (effectNext.consumePressed())
                commands.push({ToolCommandType::EffectPageNext});

            for (size_t i = 0; i < effectRows.size(); ++i)
            {
                if (effectRows[i].consumePressed() && i < view.effects.size())
                {
                    ToolCommand command;
                    command.type = ToolCommandType::OpenParticleEffect;
                    command.value = view.effects[i].path;
                    commands.push(std::move(command));
                }
            }

            for (size_t i = 0; i < emitterRows.size(); ++i)
            {
                if (emitterRows[i].consumePressed() && i < view.emitters.size())
                {
                    ToolCommand command;
                    command.type = ToolCommandType::SelectEmitter;
                    command.index = view.emitters[i].index;
                    commands.push(command);
                }
            }

            for (size_t i = 0; i < moduleRows.size(); ++i)
            {
                if (moduleRows[i].consumePressed() && i < view.modules.size())
                {
                    ToolCommand command;
                    command.type = ToolCommandType::SelectModule;
                    command.index = view.modules[i].index;
                    commands.push(command);
                }
            }
        }

        void destroy(gts::ui::UiWidgetContext& context) override
        {
            effectHeader.destroy(context);
            effectCount.destroy(context);
            for (auto& row : effectRows)
                row.destroy(context);
            effectStack.destroy(context);
            effectPrevious.destroy(context);
            effectNext.destroy(context);
            emitterHeader.destroy(context);
            for (auto& row : emitterRows)
                row.destroy(context);
            emitterStack.destroy(context);
            moduleHeader.destroy(context);
            for (auto& row : moduleRows)
                row.destroy(context);
            moduleStack.destroy(context);
            destroyRoot(context);
        }

    private:
        static constexpr size_t MaxEffectRows = 6;
        static constexpr size_t MaxEmitterRows = 5;
        static constexpr size_t MaxModuleRows = 4;

        gts::ui::UiLabelWidget effectHeader;
        gts::ui::UiLabelWidget effectCount;
        gts::ui::UiStackWidget effectStack;
        std::array<gts::ui::UiButtonWidget, MaxEffectRows> effectRows;
        gts::ui::UiButtonWidget effectPrevious;
        gts::ui::UiButtonWidget effectNext;
        gts::ui::UiLabelWidget emitterHeader;
        gts::ui::UiStackWidget emitterStack;
        std::array<gts::ui::UiButtonWidget, MaxEmitterRows> emitterRows;
        gts::ui::UiLabelWidget moduleHeader;
        gts::ui::UiStackWidget moduleStack;
        std::array<gts::ui::UiButtonWidget, MaxModuleRows> moduleRows;
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
            header.build(context,
                         content(),
                         label("Emitter Details",
                               toolui::rect(0.050f, 0.060f, 0.900f, 0.090f),
                               ToolTheme::text,
                               ToolTheme::headerTextScale));
            emitterLabel.build(context,
                               content(),
                               label("No emitter selected",
                                     toolui::rect(0.050f, 0.190f, 0.900f, 0.090f),
                                     ToolTheme::mutedText,
                                     ToolTheme::smallTextScale));
            moduleLabel.build(context,
                              content(),
                              label("No module selected",
                                    toolui::rect(0.050f, 0.300f, 0.900f, 0.090f),
                                    ToolTheme::mutedText,
                                    ToolTheme::smallTextScale));
            emitterToggle.build(context,
                                content(),
                                button("ENABLE EMITTER",
                                       toolui::rect(0.050f, 0.470f, 0.900f, 0.120f),
                                       ToolTheme::buttonTextScale));
            moduleToggle.build(context,
                               content(),
                               button("ENABLE MODULE",
                                      toolui::rect(0.050f, 0.630f, 0.900f, 0.120f),
                                      ToolTheme::buttonTextScale));
        }

        void update(gts::ui::UiWidgetContext& context,
                    const ToolShellView& view,
                    const UiLayoutSpec& layout,
                    bool visible) override
        {
            updateRoot(context, layout, visible);

            emitterLabel.setText(context, selectedEmitterText(view));
            moduleLabel.setText(context, selectedModuleText(view));
            syncButton(context,
                       emitterToggle,
                       visible,
                       view.selectedEmitterEnabled ? "DISABLE EMITTER" : "ENABLE EMITTER",
                       view.hasSelectedEmitter,
                       false);
            syncButton(context,
                       moduleToggle,
                       visible,
                       view.selectedModuleEnabled ? "DISABLE MODULE" : "ENABLE MODULE",
                       view.hasSelectedModule,
                       false);
        }

        void onEvent(gts::ui::UiWidgetContext& context,
                     UiEvent& event,
                     ToolCommandQueue& commands,
                     const ToolShellView&) override
        {
            emitterToggle.onEvent(context, event);
            moduleToggle.onEvent(context, event);

            if (emitterToggle.consumePressed())
                commands.push({ToolCommandType::ToggleEmitterEnabled});
            if (moduleToggle.consumePressed())
                commands.push({ToolCommandType::ToggleModuleEnabled});
        }

        void destroy(gts::ui::UiWidgetContext& context) override
        {
            header.destroy(context);
            emitterLabel.destroy(context);
            moduleLabel.destroy(context);
            emitterToggle.destroy(context);
            moduleToggle.destroy(context);
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

        gts::ui::UiLabelWidget header;
        gts::ui::UiLabelWidget emitterLabel;
        gts::ui::UiLabelWidget moduleLabel;
        gts::ui::UiButtonWidget emitterToggle;
        gts::ui::UiButtonWidget moduleToggle;
    };

    class ParticlePreviewViewportPane final : public ToolPaneBase
    {
    public:
        ParticlePreviewViewportPane()
            : ToolPaneBase({ToolPaneId::ParticlePreviewViewport, ToolDockArea::Right, "Particle Preview", false, true, 0})
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

            save.build(context,
                       content(),
                       button("SAVE", toolui::rect(0.050f, 0.760f, 0.205f, 0.090f), ToolTheme::buttonTextScale));
            reload.build(context,
                         content(),
                         button("RELOAD", toolui::rect(0.275f, 0.760f, 0.205f, 0.090f), ToolTheme::buttonTextScale));
            play.build(context,
                       content(),
                       button("PAUSE", toolui::rect(0.500f, 0.760f, 0.205f, 0.090f), ToolTheme::buttonTextScale));
            restart.build(context,
                          content(),
                          button("RESTART", toolui::rect(0.725f, 0.760f, 0.225f, 0.090f), ToolTheme::buttonTextScale));
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

            syncButton(context, save, visible, "SAVE", view.particleLoaded, false);
            syncButton(context, reload, visible, "RELOAD", view.particleLoaded, false);
            syncButton(context, play, visible, view.particlePlaying ? "PAUSE" : "PLAY", view.particleLoaded, false);
            syncButton(context, restart, visible, "RESTART", view.particleLoaded, false);
        }

        void onEvent(gts::ui::UiWidgetContext& context,
                     UiEvent& event,
                     ToolCommandQueue& commands,
                     const ToolShellView&) override
        {
            save.onEvent(context, event);
            reload.onEvent(context, event);
            play.onEvent(context, event);
            restart.onEvent(context, event);

            if (save.consumePressed())
                commands.push({ToolCommandType::SaveParticleEffect});
            if (reload.consumePressed())
                commands.push({ToolCommandType::ReloadParticleEffect});
            if (play.consumePressed())
                commands.push({ToolCommandType::ToggleParticlePlayback});
            if (restart.consumePressed())
                commands.push({ToolCommandType::RestartParticlePreview});
        }

        void destroy(gts::ui::UiWidgetContext& context) override
        {
            title.destroy(context);
            path.destroy(context);
            placeholder.destroy(context);
            previewImage.destroy(context);
            previewPanel.destroy(context);
            save.destroy(context);
            reload.destroy(context);
            play.destroy(context);
            restart.destroy(context);
            destroyRoot(context);
        }

    private:
        gts::ui::UiLabelWidget title;
        gts::ui::UiLabelWidget path;
        gts::ui::UiPanelWidget previewPanel;
        gts::ui::UiImageWidget previewImage;
        gts::ui::UiLabelWidget placeholder;
        gts::ui::UiButtonWidget save;
        gts::ui::UiButtonWidget reload;
        gts::ui::UiButtonWidget play;
        gts::ui::UiButtonWidget restart;
    };

    class PropertyInspectorPane final : public ToolPaneBase
    {
    public:
        PropertyInspectorPane()
            : ToolPaneBase({ToolPaneId::PropertyInspector, ToolDockArea::Right, "Inspector", true, true, 1})
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
                         label("Inspector",
                               toolui::rect(0.050f, 0.050f, 0.900f, 0.080f),
                               ToolTheme::text,
                               ToolTheme::headerTextScale));
            gts::ui::UiStackDesc lineStackDesc;
            lineStackDesc.layout = toolui::rect(0.050f, 0.155f, 0.900f, 0.790f);
            lineStackDesc.axis = UiLayoutAxis::Vertical;
            lineStackDesc.gap = 0.008f;
            lineStack.build(context, content(), lineStackDesc);
            for (auto& line : lines)
            {
                line.build(context,
                           lineStack.root(),
                           label("",
                                 stackItem(0.064f),
                                 ToolTheme::mutedText,
                                 ToolTheme::smallTextScale,
                                 UiHorizontalAlign::Left,
                                 3));
            }
        }

        void update(gts::ui::UiWidgetContext& context,
                    const ToolShellView& view,
                    const UiLayoutSpec& layout,
                    bool visible) override
        {
            updateRoot(context, layout, visible);
            header.setText(context, view.activeWorkspace == ToolWorkspace::Particles ? "Particle Inspector" : "Scene Inspector");
            for (size_t i = 0; i < lines.size(); ++i)
            {
                const bool lineVisible = visible && i < view.inspectorLines.size();
                lines[i].setVisible(context, lineVisible);
                if (lineVisible)
                    lines[i].setText(context, view.inspectorLines[i]);
            }
        }

        void onEvent(gts::ui::UiWidgetContext&, UiEvent&, ToolCommandQueue&, const ToolShellView&) override {}

        void destroy(gts::ui::UiWidgetContext& context) override
        {
            header.destroy(context);
            for (auto& line : lines)
                line.destroy(context);
            lineStack.destroy(context);
            destroyRoot(context);
        }

    private:
        static constexpr size_t MaxLines = 10;

        gts::ui::UiLabelWidget header;
        gts::ui::UiStackWidget lineStack;
        std::array<gts::ui::UiLabelWidget, MaxLines> lines;
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
            header.build(context,
                         content(),
                         label("Diagnostics",
                               toolui::rect(0.050f, 0.065f, 0.900f, 0.100f),
                               ToolTheme::text,
                               ToolTheme::smallTextScale));
            gts::ui::UiStackDesc lineStackDesc;
            lineStackDesc.layout = toolui::rect(0.050f, 0.210f, 0.900f, 0.700f);
            lineStackDesc.axis = UiLayoutAxis::Vertical;
            lineStackDesc.gap = 0.020f;
            lineStack.build(context, content(), lineStackDesc);
            for (auto& line : lines)
            {
                line.build(context,
                           lineStack.root(),
                           label("",
                                 stackItem(0.105f),
                                 ToolTheme::mutedText,
                                 ToolTheme::smallTextScale,
                                 UiHorizontalAlign::Left,
                                 2));
            }
        }

        void update(gts::ui::UiWidgetContext& context,
                    const ToolShellView& view,
                    const UiLayoutSpec& layout,
                    bool visible) override
        {
            updateRoot(context, layout, visible);
            for (size_t i = 0; i < lines.size(); ++i)
            {
                const bool lineVisible = visible && i < view.diagnostics.size();
                lines[i].setVisible(context, lineVisible);
                if (lineVisible)
                    lines[i].setText(context, view.diagnostics[i]);
            }
        }

        void onEvent(gts::ui::UiWidgetContext&, UiEvent&, ToolCommandQueue&, const ToolShellView&) override {}

        void destroy(gts::ui::UiWidgetContext& context) override
        {
            header.destroy(context);
            for (auto& line : lines)
                line.destroy(context);
            lineStack.destroy(context);
            destroyRoot(context);
        }

    private:
        static constexpr size_t MaxLines = 5;

        gts::ui::UiLabelWidget header;
        gts::ui::UiStackWidget lineStack;
        std::array<gts::ui::UiLabelWidget, MaxLines> lines;
    };

    class StatusBarPane final : public ToolPaneBase
    {
    public:
        StatusBarPane()
            : ToolPaneBase({ToolPaneId::StatusBar, ToolDockArea::Status, "Status", true, true, 0})
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
