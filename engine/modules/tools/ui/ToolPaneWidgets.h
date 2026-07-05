#pragma once

#include <array>
#include <cstddef>
#include <string>
#include <vector>

#include "EngineToolUiHelpers.h"
#include "ToolTheme.h"
#include "UiWidget.h"

namespace gts::tools
{
    inline UiLayoutSpec toolStackItem(float height)
    {
        UiLayoutSpec layout;
        layout.constraints.preferredHeight = {UiLayoutUnit::Normalized, height};
        return layout;
    }

    inline gts::ui::UiLabelDesc toolLabel(BitmapFont* font,
                                          const std::string& text,
                                          const UiLayoutSpec& layout,
                                          UiColor color,
                                          float scale,
                                          UiHorizontalAlign align = UiHorizontalAlign::Left,
                                          int maxLines = 2)
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

    inline gts::ui::UiButtonDesc toolButton(BitmapFont* font,
                                            const std::string& text,
                                            const UiLayoutSpec& layout,
                                            float scale,
                                            UiHorizontalAlign align = UiHorizontalAlign::Center)
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

    inline void paintToolButton(gts::ui::UiWidgetContext& context,
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

    inline void syncToolButton(gts::ui::UiWidgetContext& context,
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
        paintToolButton(context, widget, active);
    }

    inline std::string toolPageText(size_t offset, size_t visible, size_t total)
    {
        if (total == 0)
            return "0";
        return std::to_string(offset + 1) + "-" + std::to_string(offset + visible) + " / " +
            std::to_string(total);
    }

    struct ToolSelectableRowData
    {
        std::string icon;
        std::string primary;
        std::string secondary;
        bool visible = true;
        bool selected = false;
        bool enabled = true;
        bool warning = false;
    };

    class ToolSelectableRow
    {
    public:
        void build(gts::ui::UiWidgetContext& context,
                   UiHandle parent,
                   BitmapFont* font,
                   const UiLayoutSpec& layout,
                   float textScale = ToolTheme::smallTextScale)
        {
            row.build(context,
                      parent,
                      toolButton(font, "--", layout, textScale, UiHorizontalAlign::Left));
        }

        void sync(gts::ui::UiWidgetContext& context, const ToolSelectableRowData& data)
        {
            syncToolButton(context,
                           row,
                           data.visible,
                           data.visible ? rowText(data) : "--",
                           data.enabled,
                           data.selected);
            if (data.visible && data.warning)
                toolui::setTextColor(context.ui, context.surface, row.label(), ToolTheme::warning);
        }

        void onEvent(gts::ui::UiWidgetContext& context, UiEvent& event)
        {
            row.onEvent(context, event);
        }

        bool consumePressed()
        {
            return row.consumePressed();
        }

        UiHandle root() const { return row.root(); }
        UiHandle label() const { return row.label(); }

        void destroy(gts::ui::UiWidgetContext& context)
        {
            row.destroy(context);
        }

    private:
        static std::string rowText(const ToolSelectableRowData& data)
        {
            std::string text;
            if (!data.icon.empty())
                text = data.icon + " ";
            text += data.primary.empty() ? "--" : data.primary;
            if (!data.secondary.empty())
                text += "  " + data.secondary;
            return text;
        }

        gts::ui::UiButtonWidget row;
    };

    class ToolPager
    {
    public:
        void build(gts::ui::UiWidgetContext& context,
                   UiHandle parent,
                   BitmapFont* font,
                   const UiLayoutSpec& previousLayout,
                   const UiLayoutSpec& nextLayout,
                   float textScale = ToolTheme::buttonTextScale)
        {
            previous.build(context,
                           parent,
                           toolButton(font, "PREV", previousLayout, textScale));
            next.build(context,
                       parent,
                       toolButton(font, "NEXT", nextLayout, textScale));
        }

        void sync(gts::ui::UiWidgetContext& context, bool visible, bool hasPrevious, bool hasNext)
        {
            syncToolButton(context, previous, visible, "PREV", hasPrevious, false);
            syncToolButton(context, next, visible, "NEXT", hasNext, false);
        }

        void onEvent(gts::ui::UiWidgetContext& context, UiEvent& event)
        {
            previous.onEvent(context, event);
            next.onEvent(context, event);
        }

        bool consumePreviousPressed()
        {
            return previous.consumePressed();
        }

        bool consumeNextPressed()
        {
            return next.consumePressed();
        }

        void destroy(gts::ui::UiWidgetContext& context)
        {
            previous.destroy(context);
            next.destroy(context);
        }

    private:
        gts::ui::UiButtonWidget previous;
        gts::ui::UiButtonWidget next;
    };

    struct ToolListSectionDesc
    {
        std::string title;
        UiLayoutSpec headerLayout = gts::ui::fillLayout();
        UiLayoutSpec countLayout = gts::ui::fillLayout();
        UiLayoutSpec stackLayout = gts::ui::fillLayout();
        UiLayoutSpec previousLayout = gts::ui::fillLayout();
        UiLayoutSpec nextLayout = gts::ui::fillLayout();
        float rowHeight = ToolTheme::compactRowHeight;
        float rowGap = DefaultEditorTheme.spacing.widgetGap;
        float headerScale = ToolTheme::smallTextScale;
        float rowTextScale = ToolTheme::smallTextScale;
        bool showCount = false;
        bool showPager = false;
    };

    template <size_t RowCount>
    class ToolListSection
    {
    public:
        void build(gts::ui::UiWidgetContext& context,
                   UiHandle parent,
                   BitmapFont* font,
                   const ToolListSectionDesc& desc)
        {
            showCount = desc.showCount;
            showPager = desc.showPager;

            header.build(context,
                         parent,
                         toolLabel(font,
                                   desc.title,
                                   desc.headerLayout,
                                   ToolTheme::text,
                                   desc.headerScale));

            if (showCount)
            {
                count.build(context,
                            parent,
                            toolLabel(font,
                                      "0",
                                      desc.countLayout,
                                      ToolTheme::mutedText,
                                      ToolTheme::smallTextScale,
                                      UiHorizontalAlign::Right));
            }

            gts::ui::UiStackDesc stackDesc;
            stackDesc.layout = desc.stackLayout;
            stackDesc.axis = UiLayoutAxis::Vertical;
            stackDesc.gap = desc.rowGap;
            rowStack.build(context, parent, stackDesc);
            for (ToolSelectableRow& row : rows)
                row.build(context, rowStack.root(), font, toolStackItem(desc.rowHeight), desc.rowTextScale);

            if (showPager)
                pager.build(context, parent, font, desc.previousLayout, desc.nextLayout);
        }

        void sync(gts::ui::UiWidgetContext& context,
                  bool visible,
                  const std::vector<ToolSelectableRowData>& rowData,
                  const std::string& countText = {},
                  bool hasPrevious = false,
                  bool hasNext = false)
        {
            header.setVisible(context, visible);
            rowStack.setVisible(context, visible);
            if (showCount)
            {
                count.setVisible(context, visible);
                if (visible)
                    count.setText(context, countText);
            }

            for (size_t i = 0; i < rows.size(); ++i)
            {
                ToolSelectableRowData data;
                if (i < rowData.size())
                    data = rowData[i];
                data.visible = visible && i < rowData.size() && data.visible;
                rows[i].sync(context, data);
            }

            if (showPager)
                pager.sync(context, visible, hasPrevious, hasNext);
        }

        void onEvent(gts::ui::UiWidgetContext& context, UiEvent& event)
        {
            if (showPager)
                pager.onEvent(context, event);
            for (ToolSelectableRow& row : rows)
                row.onEvent(context, event);
        }

        bool consumePreviousPressed()
        {
            return showPager && pager.consumePreviousPressed();
        }

        bool consumeNextPressed()
        {
            return showPager && pager.consumeNextPressed();
        }

        bool consumeRowPressed(size_t& index)
        {
            for (size_t i = 0; i < rows.size(); ++i)
            {
                if (rows[i].consumePressed())
                {
                    index = i;
                    return true;
                }
            }
            return false;
        }

        void destroy(gts::ui::UiWidgetContext& context)
        {
            header.destroy(context);
            if (showCount)
                count.destroy(context);
            for (ToolSelectableRow& row : rows)
                row.destroy(context);
            rowStack.destroy(context);
            if (showPager)
                pager.destroy(context);
        }

    private:
        bool showCount = false;
        bool showPager = false;
        gts::ui::UiLabelWidget header;
        gts::ui::UiLabelWidget count;
        gts::ui::UiStackWidget rowStack;
        std::array<ToolSelectableRow, RowCount> rows;
        ToolPager pager;
    };

    struct ToolInspectorSectionDesc
    {
        std::string title;
        UiLayoutSpec headerLayout = gts::ui::fillLayout();
        UiLayoutSpec stackLayout = gts::ui::fillLayout();
        float lineHeight = ToolTheme::rowHeight;
        float lineGap = DefaultEditorTheme.spacing.widgetGap;
        float titleScale = ToolTheme::headerTextScale;
        float lineScale = ToolTheme::smallTextScale;
        UiColor lineColor = ToolTheme::mutedText;
        int lineMaxLines = 2;
    };

    template <size_t LineCount>
    class ToolInspectorSection
    {
    public:
        void build(gts::ui::UiWidgetContext& context,
                   UiHandle parent,
                   BitmapFont* font,
                   const ToolInspectorSectionDesc& desc)
        {
            header.build(context,
                         parent,
                         toolLabel(font,
                                   desc.title,
                                   desc.headerLayout,
                                   ToolTheme::text,
                                   desc.titleScale));

            gts::ui::UiStackDesc stackDesc;
            stackDesc.layout = desc.stackLayout;
            stackDesc.axis = UiLayoutAxis::Vertical;
            stackDesc.gap = desc.lineGap;
            lineStack.build(context, parent, stackDesc);
            for (gts::ui::UiLabelWidget& line : lines)
            {
                line.build(context,
                           lineStack.root(),
                           toolLabel(font,
                                     "",
                                     toolStackItem(desc.lineHeight),
                                     desc.lineColor,
                                     desc.lineScale,
                                     UiHorizontalAlign::Left,
                                     desc.lineMaxLines));
            }
        }

        void sync(gts::ui::UiWidgetContext& context,
                  bool visible,
                  const std::string& title,
                  const std::vector<std::string>& textLines)
        {
            header.setVisible(context, visible);
            lineStack.setVisible(context, visible);
            if (visible)
                header.setText(context, title);

            for (size_t i = 0; i < lines.size(); ++i)
            {
                const bool lineVisible = visible && i < textLines.size();
                lines[i].setVisible(context, lineVisible);
                if (lineVisible)
                    lines[i].setText(context, textLines[i]);
            }
        }

        void destroy(gts::ui::UiWidgetContext& context)
        {
            header.destroy(context);
            for (gts::ui::UiLabelWidget& line : lines)
                line.destroy(context);
            lineStack.destroy(context);
        }

    private:
        gts::ui::UiLabelWidget header;
        gts::ui::UiStackWidget lineStack;
        std::array<gts::ui::UiLabelWidget, LineCount> lines;
    };

    struct ToolToolbarButtonSlot
    {
        std::string text;
        UiLayoutSpec layout = gts::ui::fillLayout();
        UiHorizontalAlign align = UiHorizontalAlign::Center;
    };

    template <size_t ButtonCount>
    class ToolToolbarRow
    {
    public:
        void build(gts::ui::UiWidgetContext& context,
                   UiHandle parent,
                   BitmapFont* font,
                   const std::array<ToolToolbarButtonSlot, ButtonCount>& slots,
                   float textScale = ToolTheme::buttonTextScale)
        {
            for (size_t i = 0; i < buttons.size(); ++i)
            {
                buttons[i].build(context,
                                 parent,
                                 toolButton(font, slots[i].text, slots[i].layout, textScale, slots[i].align));
            }
        }

        void sync(gts::ui::UiWidgetContext& context,
                  size_t index,
                  bool visible,
                  const std::string& text,
                  bool enabled,
                  bool active)
        {
            if (index >= buttons.size())
                return;
            syncToolButton(context, buttons[index], visible, text, enabled, active);
        }

        void onEvent(gts::ui::UiWidgetContext& context, UiEvent& event)
        {
            for (gts::ui::UiButtonWidget& button : buttons)
                button.onEvent(context, event);
        }

        bool consumePressed(size_t index)
        {
            return index < buttons.size() && buttons[index].consumePressed();
        }

        void destroy(gts::ui::UiWidgetContext& context)
        {
            for (gts::ui::UiButtonWidget& button : buttons)
                button.destroy(context);
        }

    private:
        std::array<gts::ui::UiButtonWidget, ButtonCount> buttons;
    };
} // namespace gts::tools
