#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

#include "EngineToolUiHelpers.h"
#include "ToolPaneWidgets.h"
#include "ToolPropertyTypes.h"
#include "ToolTheme.h"
#include "UiWidget.h"

namespace gts::tools
{
    class ToolPropertyRow
    {
    public:
        void build(gts::ui::UiWidgetContext& context,
                   UiHandle parent,
                   BitmapFont* font,
                   const UiLayoutSpec& layout)
        {
            gts::ui::UiPanelDesc desc;
            desc.layout = layout;
            desc.styleClass.clear();
            desc.enabled = true;
            desc.interactable = false;
            rowPanel.build(context, parent, desc);
            toolui::setPanelPayload(context.ui,
                                    context.surface,
                                    rowPanel.root(),
                                    ToolTheme::inspectorRowBackground,
                                    ToolTheme::borderSubtle,
                                    ToolTheme::panelBorderWidth,
                                    {0.0f, 0.0f, 0.0f, 0.0f},
                                    {},
                                    ToolTheme::rowRadius);

            gts::ui::UiPanelDesc railDesc;
            railDesc.layout = toolui::rect(0.000f, 0.120f, 0.010f, 0.760f);
            railDesc.styleClass.clear();
            railDesc.enabled = true;
            railDesc.interactable = false;
            rail.build(context, rowPanel.content(), railDesc);
            toolui::setRectPayload(context.ui, context.surface, rail.root(), ToolTheme::borderSubtle);

            name.build(context,
                       rowPanel.content(),
                       toolLabel(font,
                                 "",
                                 toolui::rect(0.035f, 0.120f, 0.385f, 0.760f),
                                 ToolTheme::mutedText,
                                 ToolTheme::smallTextScale));
            value.build(context,
                        rowPanel.content(),
                        toolLabel(font,
                                  "",
                                  toolui::rect(0.430f, 0.120f, 0.275f, 0.760f),
                                  ToolTheme::text,
                                  ToolTheme::smallTextScale,
                                  UiHorizontalAlign::Right));

            std::array<ToolToolbarButtonSlot, 1> toggleSlots;
            toggleSlots[0] = {"--", toolui::rect(0.735f, 0.130f, 0.235f, 0.740f)};
            toggle.build(context, rowPanel.content(), font, toggleSlots, ToolTheme::smallTextScale);

            std::array<ToolToolbarButtonSlot, 2> stepperSlots;
            stepperSlots[0] = {"-", toolui::rect(0.735f, 0.130f, 0.105f, 0.740f)};
            stepperSlots[1] = {"+", toolui::rect(0.865f, 0.130f, 0.105f, 0.740f)};
            stepper.build(context, rowPanel.content(), font, stepperSlots, ToolTheme::smallTextScale);
        }

        void sync(gts::ui::UiWidgetContext& context, bool rowVisible, const ToolPropertyDescriptor& descriptor)
        {
            visible = rowVisible;
            property = descriptor;

            rowPanel.setVisible(context, visible);
            rail.setVisible(context, visible);
            name.setVisible(context, visible);
            value.setVisible(context, visible);
            if (visible)
            {
                name.setText(context, property.displayName.empty() ? property.id : property.displayName);
                value.setText(context, valueText(property));
            }

            const bool editable = visible && property.enabled && !property.readOnly;
            const UiColor fill = editable ? ToolTheme::cardBackground : ToolTheme::inspectorRowBackground;
            const UiColor railColor = editable ? ToolTheme::rowAccent : ToolTheme::borderSubtle;
            toolui::setPanelPayload(context.ui,
                                    context.surface,
                                    rowPanel.root(),
                                    fill,
                                    editable ? ToolTheme::accentMuted : ToolTheme::borderSubtle,
                                    ToolTheme::panelBorderWidth,
                                    {0.0f, 0.0f, 0.0f, 0.0f},
                                    {},
                                    ToolTheme::rowRadius);
            toolui::setRectPayload(context.ui, context.surface, rail.root(), railColor);
            toolui::setTextColor(context.ui, context.surface, name.root(), editable ? ToolTheme::text : ToolTheme::mutedText);
            toolui::setTextColor(context.ui, context.surface, value.root(), property.enabled ? ToolTheme::text : ToolTheme::disabledText);

            const bool showToggle = editable && (property.kind == ToolPropertyKind::Bool ||
                                                 property.kind == ToolPropertyKind::Enum);
            const bool showStepper = editable && (property.kind == ToolPropertyKind::Float ||
                                                  property.kind == ToolPropertyKind::Int ||
                                                  property.kind == ToolPropertyKind::UInt);

            toggle.sync(context,
                        0,
                        showToggle,
                        property.kind == ToolPropertyKind::Bool ? (property.boolValue ? "ON" : "OFF") : "NEXT",
                        showToggle,
                        property.kind == ToolPropertyKind::Bool && property.boolValue);
            stepper.sync(context, 0, showStepper, "-", showStepper, false);
            stepper.sync(context, 1, showStepper, "+", showStepper, false);
        }

        void onEvent(gts::ui::UiWidgetContext& context, UiEvent& event, ToolCommandQueue& commands)
        {
            toggle.onEvent(context, event);
            stepper.onEvent(context, event);

            const bool togglePressed = toggle.consumePressed(0);
            const bool decrementPressed = stepper.consumePressed(0);
            const bool incrementPressed = stepper.consumePressed(1);

            if (!visible || !property.enabled || property.readOnly)
                return;

            if (togglePressed)
            {
                if (property.kind == ToolPropertyKind::Bool)
                    emitBool(commands, !property.boolValue);
                else if (property.kind == ToolPropertyKind::Enum)
                    emitEnum(commands);
            }

            if (decrementPressed)
                emitStepped(commands, -1);
            if (incrementPressed)
                emitStepped(commands, 1);
        }

        void destroy(gts::ui::UiWidgetContext& context)
        {
            name.destroy(context);
            value.destroy(context);
            toggle.destroy(context);
            stepper.destroy(context);
            rail.destroy(context);
            rowPanel.destroy(context);
        }

    private:
        static std::string valueText(const ToolPropertyDescriptor& descriptor)
        {
            switch (descriptor.kind)
            {
                case ToolPropertyKind::Label:
                case ToolPropertyKind::ReadOnlyText:
                    return descriptor.textValue;
                case ToolPropertyKind::Bool:
                    return descriptor.boolValue ? "true" : "false";
                case ToolPropertyKind::Float:
                    return toolui::fixed(descriptor.floatValue);
                case ToolPropertyKind::Int:
                    return std::to_string(descriptor.intValue);
                case ToolPropertyKind::UInt:
                    return std::to_string(descriptor.uintValue);
                case ToolPropertyKind::Enum:
                    for (const ToolPropertyEnumValue& option : descriptor.enumValues)
                    {
                        if (option.value == descriptor.intValue)
                            return option.label.empty() ? option.id : option.label;
                    }
                    return std::to_string(descriptor.intValue);
            }
            return {};
        }

        ToolCommand baseCommand() const
        {
            ToolCommand command = property.command;
            if (command.value.empty())
                command.value = property.id;
            return command;
        }

        void emitBool(ToolCommandQueue& commands, bool value) const
        {
            ToolCommand command = baseCommand();
            command.boolValue = value;
            commands.push(command);
        }

        void emitEnum(ToolCommandQueue& commands) const
        {
            if (property.enumValues.empty())
                return;

            size_t index = 0;
            for (size_t i = 0; i < property.enumValues.size(); ++i)
            {
                if (property.enumValues[i].value == property.intValue)
                {
                    index = (i + 1) % property.enumValues.size();
                    break;
                }
            }

            ToolCommand command = baseCommand();
            command.intValue = property.enumValues[index].value;
            command.uintValue = static_cast<uint32_t>(std::max(0, command.intValue));
            commands.push(command);
        }

        void emitStepped(ToolCommandQueue& commands, int direction) const
        {
            switch (property.kind)
            {
                case ToolPropertyKind::Float:
                {
                    const float step = property.floatStep <= 0.0f ? 0.1f : property.floatStep;
                    const float nextValue = std::clamp(property.floatValue + step * static_cast<float>(direction),
                                                       property.minFloat,
                                                       property.maxFloat);
                    if (std::fabs(nextValue - property.floatValue) <= 0.0001f)
                        return;

                    ToolCommand command = baseCommand();
                    command.floatValue = nextValue;
                    commands.push(command);
                    return;
                }
                case ToolPropertyKind::Int:
                {
                    const int32_t step = property.intStep <= 0 ? 1 : property.intStep;
                    const int32_t nextValue = std::clamp(property.intValue + step * direction,
                                                         property.minInt,
                                                         property.maxInt);
                    if (nextValue == property.intValue)
                        return;

                    ToolCommand command = baseCommand();
                    command.intValue = nextValue;
                    command.uintValue = static_cast<uint32_t>(std::max(0, nextValue));
                    commands.push(command);
                    return;
                }
                case ToolPropertyKind::UInt:
                {
                    const uint32_t step = property.uintStep == 0 ? 1u : property.uintStep;
                    const int64_t signedValue = static_cast<int64_t>(property.uintValue) +
                        static_cast<int64_t>(step) * static_cast<int64_t>(direction);
                    const uint32_t nextValue = static_cast<uint32_t>(
                        std::clamp<int64_t>(signedValue, property.minUInt, property.maxUInt));
                    if (nextValue == property.uintValue)
                        return;

                    ToolCommand command = baseCommand();
                    command.uintValue = nextValue;
                    command.intValue = static_cast<int32_t>(
                        std::min<uint32_t>(nextValue, std::numeric_limits<int32_t>::max()));
                    commands.push(command);
                    return;
                }
                default:
                    return;
            }
        }

        bool visible = false;
        ToolPropertyDescriptor property;
        gts::ui::UiPanelWidget rowPanel;
        gts::ui::UiPanelWidget rail;
        gts::ui::UiLabelWidget name;
        gts::ui::UiLabelWidget value;
        ToolToolbarRow<1> toggle;
        ToolToolbarRow<2> stepper;
    };

    template <size_t MaxSections, size_t MaxRowsPerSection>
    class ToolPropertyInspector
    {
    public:
        void build(gts::ui::UiWidgetContext& context,
                   UiHandle parent,
                   BitmapFont* font,
                   const UiLayoutSpec& layout)
        {
            gts::ui::UiStackDesc stackDesc;
            stackDesc.layout = layout;
            stackDesc.axis = UiLayoutAxis::Vertical;
            stackDesc.gap = DefaultEditorTheme.spacing.widgetGap;
            stack.build(context, parent, stackDesc);

            for (size_t sectionIndex = 0; sectionIndex < MaxSections; ++sectionIndex)
            {
                gts::ui::UiPanelDesc headerDesc;
                headerDesc.layout = toolStackItem(ToolTheme::panelHeaderHeight);
                headerDesc.styleClass.clear();
                headerDesc.enabled = true;
                headerDesc.interactable = false;
                sectionHeaderPanels[sectionIndex].build(context, stack.root(), headerDesc);
                toolui::setSurfacePayload(context.ui,
                                          context.surface,
                                          sectionHeaderPanels[sectionIndex].root(),
                                          toolui::SurfaceRole::SectionHeader);
                sectionHeaders[sectionIndex].build(context,
                                                   sectionHeaderPanels[sectionIndex].content(),
                                                   toolLabel(font,
                                                             "",
                                                             toolui::rect(0.030f, 0.080f, 0.940f, 0.840f),
                                                             ToolTheme::text,
                                                             ToolTheme::headerTextScale));
                for (ToolPropertyRow& row : rows[sectionIndex])
                    row.build(context, stack.root(), font, toolStackItem(ToolTheme::propertyRowHeight));
            }
        }

        void sync(gts::ui::UiWidgetContext& context,
                  bool visible,
                  const std::vector<ToolPropertySection>& sections)
        {
            stack.setVisible(context, visible);
            currentSections = sections;

            for (size_t sectionIndex = 0; sectionIndex < MaxSections; ++sectionIndex)
            {
                const bool sectionVisible = visible &&
                    sectionIndex < currentSections.size() &&
                    !currentSections[sectionIndex].properties.empty();
                sectionHeaders[sectionIndex].setVisible(context, sectionVisible);
                sectionHeaderPanels[sectionIndex].setVisible(context, sectionVisible);
                if (sectionVisible)
                    sectionHeaders[sectionIndex].setText(context, currentSections[sectionIndex].title);

                for (size_t rowIndex = 0; rowIndex < MaxRowsPerSection; ++rowIndex)
                {
                    ToolPropertyDescriptor descriptor;
                    const bool rowVisible = sectionVisible &&
                        rowIndex < currentSections[sectionIndex].properties.size();
                    if (rowVisible)
                        descriptor = currentSections[sectionIndex].properties[rowIndex];
                    rows[sectionIndex][rowIndex].sync(context, rowVisible, descriptor);
                }
            }
        }

        void onEvent(gts::ui::UiWidgetContext& context,
                     UiEvent& event,
                     ToolCommandQueue& commands)
        {
            for (auto& sectionRows : rows)
            {
                for (ToolPropertyRow& row : sectionRows)
                    row.onEvent(context, event, commands);
            }
        }

        void destroy(gts::ui::UiWidgetContext& context)
        {
            for (gts::ui::UiLabelWidget& header : sectionHeaders)
                header.destroy(context);
            for (gts::ui::UiPanelWidget& panel : sectionHeaderPanels)
                panel.destroy(context);
            for (auto& sectionRows : rows)
            {
                for (ToolPropertyRow& row : sectionRows)
                    row.destroy(context);
            }
            stack.destroy(context);
        }

    private:
        gts::ui::UiStackWidget stack;
        std::array<gts::ui::UiPanelWidget, MaxSections> sectionHeaderPanels;
        std::array<gts::ui::UiLabelWidget, MaxSections> sectionHeaders;
        std::array<std::array<ToolPropertyRow, MaxRowsPerSection>, MaxSections> rows;
        std::vector<ToolPropertySection> currentSections;
    };
} // namespace gts::tools
