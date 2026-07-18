#pragma once

#include <cstddef>
#include <string>

#include "EngineToolUiHelpers.h"
#include "ToolPane.h"
#include "ToolPaneWidgets.h"
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
            if (color.a > 0.0f)
            {
                toolui::setPanelPayload(context.ui,
                                        context.surface,
                                        rootPanel.root(),
                                        color,
                                        ToolTheme::border,
                                        ToolTheme::panelBorderWidth,
                                        ToolTheme::shadow,
                                        {0.0f, ToolTheme::panelShadowOffset},
                                        ToolTheme::panelRadius,
                                        DefaultEditorTheme.shadow.blur);
            }
            else
            {
                toolui::setRectPayload(context.ui, context.surface, rootPanel.root(), color);
            }
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
                               bool interactable = false,
                               bool framed = false,
                               bool shadow = false,
                               float cornerRadius = ToolTheme::radiusMedium)
        {
            gts::ui::UiPanelDesc desc;
            desc.layout = layout;
            desc.styleClass.clear();
            desc.enabled = true;
            desc.interactable = interactable;
            panel.build(context, parent, desc);
            if (framed)
            {
                toolui::setPanelPayload(context.ui,
                                        context.surface,
                                        panel.root(),
                                        color,
                                        ToolTheme::borderSubtle,
                                        ToolTheme::panelBorderWidth,
                                        shadow ? ToolTheme::shadow : transparent(),
                                        shadow ? UiVec2{0.0f, ToolTheme::panelShadowOffset}
                                               : UiVec2{},
                                        cornerRadius,
                                        shadow ? DefaultEditorTheme.shadow.blur * 0.70f : 0.0f);
            }
            else
            {
                toolui::setRectPayload(context.ui, context.surface, panel.root(), color);
            }
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

} // namespace gts::tools
