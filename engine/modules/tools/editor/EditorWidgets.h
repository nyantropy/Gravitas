#pragma once

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

#include "EditorTheme.h"
#include "ToolWidgets.h"
#include "UiSystem.h"

namespace gts::tools
{
    enum class EditorPropertyWidgetKind
    {
        Text,
        Bool,
        Integer,
        Float,
        Range,
        Enum,
        Asset,
        Color,
        Curve,
        Gradient,
        Tags
    };

    enum class EditorValidationSeverity
    {
        Info,
        Success,
        Warning,
        Error
    };

    struct EditorWidgetHandles
    {
        UiHandle                   root = UI_INVALID_HANDLE;
        std::vector<UiHandle>      surfaces;
        std::vector<UiHandle>      labels;
        std::vector<ToolButton>    buttons;
        std::vector<ToolTextField> fields;
        std::vector<ToolSlider>    sliders;
    };

    struct EditorTreeNodeSpec
    {
        std::string id;
        std::string label;
        std::string detail;
        int         depth    = 0;
        bool        expanded = true;
        bool        selected = false;
        bool        visible  = true;
    };

    struct EditorPropertySpec
    {
        std::string              id;
        std::string              displayName;
        std::string              value;
        std::string              tooltip;
        std::string              category;
        EditorPropertyWidgetKind widget   = EditorPropertyWidgetKind::Text;
        bool                     readOnly = false;
        bool                     modified = false;
        bool                     visible  = true;
    };

    struct EditorMenuItemSpec
    {
        std::string id;
        std::string label;
        std::string shortcut;
        bool        enabled   = true;
        bool        checked   = false;
        bool        separator = false;
    };

    struct EditorAssetSpec
    {
        std::string id;
        std::string name;
        std::string type;
        bool        selected = false;
        bool        visible  = true;
    };

    struct EditorValidationMessageSpec
    {
        EditorValidationSeverity severity = EditorValidationSeverity::Info;
        std::string              source;
        std::string              message;
    };

    struct EditorGraphNodeSpec
    {
        std::string id;
        std::string label;
        UiRect      rect;
        bool        selected = false;
    };

    struct EditorGraphLinkSpec
    {
        size_t fromIndex = 0;
        size_t toIndex   = 0;
    };

    struct EditorTimelineTrackSpec
    {
        std::string        id;
        std::string        label;
        std::vector<float> keys;
    };

    struct EditorCurveKeySpec
    {
        float time     = 0.0f;
        float value    = 0.0f;
        bool  selected = false;
    };

    struct EditorGradientKeySpec
    {
        float   position = 0.0f;
        UiColor color    = {1.0f, 1.0f, 1.0f, 1.0f};
        bool    selected = false;
    };

    inline UiColor validationColor(EditorValidationSeverity severity, const EditorTheme& theme = DefaultEditorTheme)
    {
        switch (severity)
        {
        case EditorValidationSeverity::Info:
            return theme.colors.info;
        case EditorValidationSeverity::Success:
            return theme.colors.success;
        case EditorValidationSeverity::Warning:
            return theme.colors.warning;
        case EditorValidationSeverity::Error:
            return theme.colors.error;
        }
        return theme.colors.info;
    }

    inline UiHandle createEditorCircleRelative(
        UiSystem& ui, UiHandle parent, const ToolRect& rect, UiColor color, bool interactable = false)
    {
        UiHandle handle = ui.createNode(UiNodeType::Circle, parent);
        ui.setLayout(handle, relativeLayout(rect));
        ui.setState(handle, UiStateFlags{.visible = true, .enabled = true, .interactable = interactable});
        ui.setPayload(handle, UiCircleData{color, 24});
        return handle;
    }

    inline UiHandle createEditorLineRelative(
        UiSystem& ui, UiHandle parent, UiVec2 start, UiVec2 end, UiColor color, float thickness = 0.002f)
    {
        UiHandle handle = ui.createNode(UiNodeType::Line, parent);
        ui.setLayout(handle, relativeLayout({0.0f, 0.0f, 1.0f, 1.0f}));
        ui.setState(handle, UiStateFlags{.visible = true, .enabled = false, .interactable = false});
        ui.setPayload(handle, UiLineData{start, end, color, thickness});
        return handle;
    }

    inline EditorWidgetHandles createEditorSearchField(UiSystem&          ui,
                                                       UiHandle           parent,
                                                       const ToolRect&    rect,
                                                       std::string_view   label,
                                                       std::string_view   value,
                                                       BitmapFont*        font,
                                                       const EditorTheme& theme = DefaultEditorTheme)
    {
        EditorWidgetHandles handles;
        handles.root = createContainerRelative(ui, parent, rect);
        ToolTextField field =
            createSearchFieldRelative(ui, handles.root, {0.0f, 0.0f, 1.0f, 1.0f}, std::string(label), font);
        updateTextField(ui, field, std::string(value), false);
        handles.fields.push_back(field);
        handles.surfaces.push_back(field.rect);
        return handles;
    }

    inline EditorWidgetHandles createEditorBreadcrumbBar(UiSystem&                       ui,
                                                         UiHandle                        parent,
                                                         const ToolRect&                 rect,
                                                         const std::vector<std::string>& crumbs,
                                                         BitmapFont*                     font,
                                                         const EditorTheme&              theme = DefaultEditorTheme)
    {
        EditorWidgetHandles handles;
        handles.root = createRectRelative(ui, parent, rect, theme.colors.panelBackground);
        if (crumbs.empty())
            return handles;

        const float gap = theme.spacing.xs;
        const float width =
            std::max(0.02f, (1.0f - gap * static_cast<float>(crumbs.size() - 1)) / static_cast<float>(crumbs.size()));
        for (size_t i = 0; i < crumbs.size(); ++i)
        {
            const float x      = static_cast<float>(i) * (width + gap);
            ToolButton  button = createButtonRelative(
                ui, handles.root, {x, 0.0f, width, 1.0f}, font, crumbs[i], theme.typography.smallScale);
            handles.buttons.push_back(button);
        }
        return handles;
    }

    inline EditorWidgetHandles createEditorToolbarButton(UiSystem&          ui,
                                                         UiHandle           parent,
                                                         const ToolRect&    rect,
                                                         std::string_view   label,
                                                         BitmapFont*        font,
                                                         bool               active = false,
                                                         const EditorTheme& theme  = DefaultEditorTheme)
    {
        EditorWidgetHandles handles;
        ToolButton          button =
            createButtonRelative(ui, parent, rect, font, std::string(label), theme.typography.buttonScale);
        if (active)
        {
            setRectColor(ui, button.rect, theme.colors.selection);
            createRectRelative(ui, button.rect, {0.0f, 0.0f, 1.0f, 0.060f}, theme.colors.accent);
        }
        handles.root = button.rect;
        handles.buttons.push_back(button);
        return handles;
    }

    inline EditorWidgetHandles createEditorIconButton(UiSystem&          ui,
                                                      UiHandle           parent,
                                                      const ToolRect&    rect,
                                                      std::string_view   iconId,
                                                      BitmapFont*        font,
                                                      const EditorTheme& theme = DefaultEditorTheme)
    {
        return createEditorToolbarButton(ui, parent, rect, iconId, font, false, theme);
    }

    inline EditorWidgetHandles createEditorTreeView(UiSystem&                              ui,
                                                    UiHandle                               parent,
                                                    const ToolRect&                        rect,
                                                    const std::vector<EditorTreeNodeSpec>& nodes,
                                                    BitmapFont*                            font,
                                                    const EditorTheme&                     theme = DefaultEditorTheme)
    {
        EditorWidgetHandles handles;
        handles.root = createRectRelative(ui, parent, rect, theme.colors.panelInset);

        UiLayoutSpec rootLayout = relativeLayout(rect);
        rootLayout.clipMode     = UiClipMode::ClipChildren;
        ui.setLayout(handles.root, rootLayout);

        float       y         = theme.spacing.sm;
        const float rowHeight = theme.dimensions.compactRowHeight;
        for (const EditorTreeNodeSpec& node : nodes)
        {
            if (!node.visible)
                continue;

            UiColor     rowColor = node.selected ? theme.colors.selection : theme.colors.panelInset;
            UiHandle    row      = createRectRelative(ui, handles.root, {0.012f, y, 0.976f, rowHeight}, rowColor, true);
            const float indent   = std::min(0.30f, 0.030f * static_cast<float>(std::max(0, node.depth)));
            if (node.depth > 0)
            {
                createEditorLineRelative(ui,
                                         row,
                                         {0.030f + indent - 0.014f, 0.0f},
                                         {0.030f + indent - 0.014f, 1.0f},
                                         theme.colors.borderSubtle,
                                         0.0010f);
            }
            UiHandle disclosure = createTextRelative(ui,
                                                     row,
                                                     {0.018f + indent, 0.12f, 0.035f, 0.76f},
                                                     font,
                                                     node.expanded ? "v" : ">",
                                                     theme.colors.iconMuted,
                                                     theme.typography.metadataScale);
            setTextAlignment(ui, disclosure, UiHorizontalAlign::Center, UiVerticalAlign::Middle);
            UiHandle icon = createTextRelative(ui,
                                               row,
                                               {0.054f + indent, 0.12f, 0.040f, 0.76f},
                                               font,
                                               node.depth == 0 ? "*" : "o",
                                               node.selected ? theme.colors.textPrimary : theme.colors.icon,
                                               theme.typography.metadataScale);
            setTextAlignment(ui, icon, UiHorizontalAlign::Center, UiVerticalAlign::Middle);
            UiHandle    label    = createTextRelative(ui,
                                                      row,
                                                      {0.100f + indent, 0.12f, 0.52f - indent, 0.76f},
                                                      font,
                                                      node.label,
                                                      theme.colors.text,
                                                      theme.typography.valueScale);
            UiHandle    detail   = createTextRelative(ui,
                                                      row,
                                                      {0.68f, 0.12f, 0.25f, 0.76f},
                                                      font,
                                                      node.detail,
                                                      theme.colors.mutedText,
                                                      theme.typography.metadataScale);
            setTextAlignment(ui, detail, UiHorizontalAlign::Right, UiVerticalAlign::Middle);
            handles.surfaces.push_back(row);
            handles.labels.push_back(label);
            handles.labels.push_back(detail);
            y += rowHeight + theme.spacing.xs;
        }

        return handles;
    }

    inline EditorWidgetHandles createEditorHierarchyView(UiSystem&                              ui,
                                                         UiHandle                               parent,
                                                         const ToolRect&                        rect,
                                                         const std::vector<EditorTreeNodeSpec>& nodes,
                                                         BitmapFont*                            font,
                                                         const EditorTheme& theme = DefaultEditorTheme)
    {
        return createEditorTreeView(ui, parent, rect, nodes, font, theme);
    }

    inline EditorWidgetHandles createEditorPropertyGrid(UiSystem&                              ui,
                                                        UiHandle                               parent,
                                                        const ToolRect&                        rect,
                                                        const std::vector<EditorPropertySpec>& properties,
                                                        BitmapFont*                            font,
                                                        const EditorTheme& theme = DefaultEditorTheme)
    {
        EditorWidgetHandles handles;
        handles.root = createRectRelative(ui, parent, rect, theme.colors.panelInset);
        UiLayoutSpec rootLayout = relativeLayout(rect);
        rootLayout.clipMode     = UiClipMode::ClipChildren;
        ui.setLayout(handles.root, rootLayout);

        float       y         = theme.spacing.sm;
        const float rowHeight = theme.dimensions.propertyRowHeight;
        std::string currentCategory;
        for (const EditorPropertySpec& property : properties)
        {
            if (!property.visible)
                continue;

            if (property.category != currentCategory)
            {
                currentCategory = property.category;
                const std::string header = currentCategory.empty() ? "Properties" : currentCategory;
                UiHandle headerRow = createRectRelative(
                    ui, handles.root, {0.012f, y, 0.976f, theme.dimensions.compactRowHeight}, theme.colors.sectionHeader);
                createRectRelative(ui, headerRow, {0.0f, 0.0f, 1.0f, 0.050f}, theme.colors.borderSubtle);
                UiHandle headerLabel = createTextRelative(ui,
                                                          headerRow,
                                                          {0.025f, 0.12f, 0.70f, 0.76f},
                                                          font,
                                                          header,
                                                          theme.colors.textPrimary,
                                                          theme.typography.sectionHeaderScale);
                setTextAlignment(ui, headerLabel, UiHorizontalAlign::Left, UiVerticalAlign::Middle);
                y += theme.dimensions.compactRowHeight + theme.spacing.xs;
            }

            const UiColor rowColor = property.modified ? theme.colors.accentSoft : theme.colors.cardBackground;
            UiHandle      row      = createRectRelative(ui, handles.root, {0.012f, y, 0.976f, rowHeight}, rowColor);
            createRectRelative(ui, row, {0.0f, 0.0f, 1.0f, 0.030f}, theme.colors.highlight);
            UiHandle      label    = createTextRelative(ui,
                                                        row,
                                                        {0.025f, 0.12f, 0.36f, 0.76f},
                                                        font,
                                                        property.displayName,
                                                        property.readOnly ? theme.colors.textDisabled : theme.colors.textSecondary,
                                                        theme.typography.labelScale);
            ToolButton    value    = createButtonRelative(
                ui, row, {0.420f, 0.12f, 0.430f, 0.76f}, font, property.value, theme.typography.valueScale);
            if (property.readOnly)
            {
                setRectColor(ui, value.rect, theme.colors.disabled);
                setTextColor(ui, value.label, theme.colors.textDisabled);
            }
            if (property.modified)
            {
                createRectRelative(ui, row, {0.875f, 0.30f, 0.025f, 0.40f}, theme.colors.accent);
            }
            ToolButton reset = createButtonRelative(
                ui, row, {0.915f, 0.12f, 0.060f, 0.76f}, font, "R", theme.typography.metadataScale);
            setRectColor(ui, reset.rect, theme.colors.buttonSecondary);
            setTextColor(ui, reset.label, property.modified ? theme.colors.icon : theme.colors.iconMuted);

            handles.surfaces.push_back(row);
            handles.labels.push_back(label);
            handles.buttons.push_back(value);
            handles.buttons.push_back(reset);
            y += rowHeight + theme.spacing.xs;
        }

        return handles;
    }

    inline EditorWidgetHandles createEditorInspector(UiSystem&                              ui,
                                                     UiHandle                               parent,
                                                     const ToolRect&                        rect,
                                                     std::string_view                       title,
                                                     const std::vector<EditorPropertySpec>& properties,
                                                     BitmapFont*                            font,
                                                     const EditorTheme&                     theme = DefaultEditorTheme)
    {
        EditorWidgetHandles handles;
        handles.root = createRectRelative(ui, parent, rect, theme.colors.panelBackground);
        createRectRelative(ui, handles.root, {0.0f, 0.0f, 1.0f, 0.085f}, theme.colors.headerBackground);
        createRectRelative(ui, handles.root, {0.0f, 0.083f, 1.0f, 0.004f}, theme.colors.separator);
        handles.labels.push_back(createTextRelative(ui,
                                                    handles.root,
                                                    {0.025f, 0.020f, 0.950f, 0.052f},
                                                    font,
                                                    std::string(title),
                                                    theme.colors.text,
                                                    theme.typography.headerScale));
        EditorWidgetHandles grid =
            createEditorPropertyGrid(ui, handles.root, {0.018f, 0.105f, 0.964f, 0.870f}, properties, font, theme);
        handles.surfaces.push_back(grid.root);
        handles.buttons.insert(handles.buttons.end(), grid.buttons.begin(), grid.buttons.end());
        handles.labels.insert(handles.labels.end(), grid.labels.begin(), grid.labels.end());
        return handles;
    }

    inline EditorWidgetHandles createEditorFoldoutSection(UiSystem&          ui,
                                                          UiHandle           parent,
                                                          const ToolRect&    rect,
                                                          std::string_view   label,
                                                          std::string_view   summary,
                                                          bool               expanded,
                                                          BitmapFont*        font,
                                                          const EditorTheme& theme = DefaultEditorTheme)
    {
        EditorWidgetHandles handles;
        ToolSectionHeader   header =
            createSectionHeaderRelative(ui, parent, rect, font, std::string(label), std::string(summary));
        updateSectionHeader(ui, header, std::string(label), std::string(summary), expanded);
        handles.root = header.rect;
        handles.surfaces.push_back(header.rect);
        handles.labels.push_back(header.label);
        handles.labels.push_back(header.summary);
        return handles;
    }

    inline EditorWidgetHandles createEditorMenu(UiSystem&                              ui,
                                                UiHandle                               parent,
                                                const ToolRect&                        rect,
                                                const std::vector<EditorMenuItemSpec>& items,
                                                BitmapFont*                            font,
                                                const EditorTheme&                     theme = DefaultEditorTheme)
    {
        EditorWidgetHandles handles;
        handles.root          = createRectRelative(ui, parent, rect, theme.colors.menuBackground);
        float       y         = theme.spacing.xs;
        const float rowHeight = theme.dimensions.compactRowHeight;
        for (const EditorMenuItemSpec& item : items)
        {
            if (item.separator)
            {
                handles.surfaces.push_back(createRectRelative(
                    ui, handles.root, {0.025f, y + rowHeight * 0.45f, 0.950f, 0.004f}, theme.colors.separator));
                y += rowHeight * 0.65f;
                continue;
            }

            ToolButton button = createButtonRelative(
                ui, handles.root, {0.025f, y, 0.950f, rowHeight}, font, item.label, theme.typography.smallScale);
            if (!item.enabled)
                setRectColor(ui, button.rect, theme.colors.disabled);
            if (item.checked)
                setRectColor(ui, button.rect, theme.colors.selection);
            if (!item.shortcut.empty())
            {
                UiHandle shortcut = createTextRelative(ui,
                                                       button.rect,
                                                       {0.68f, 0.12f, 0.29f, 0.76f},
                                                       font,
                                                       item.shortcut,
                                                       theme.colors.mutedText,
                                                       theme.typography.smallScale);
                setTextAlignment(ui, shortcut, UiHorizontalAlign::Right, UiVerticalAlign::Middle);
                handles.labels.push_back(shortcut);
            }
            handles.buttons.push_back(button);
            y += rowHeight + theme.spacing.xs;
        }
        return handles;
    }

    inline EditorWidgetHandles createEditorContextMenu(UiSystem&                              ui,
                                                       UiHandle                               parent,
                                                       const ToolRect&                        rect,
                                                       const std::vector<EditorMenuItemSpec>& items,
                                                       BitmapFont*                            font,
                                                       const EditorTheme& theme = DefaultEditorTheme)
    {
        return createEditorMenu(ui, parent, rect, items, font, theme);
    }

    inline EditorWidgetHandles createEditorPopupMenu(UiSystem&                              ui,
                                                     UiHandle                               parent,
                                                     const ToolRect&                        rect,
                                                     const std::vector<EditorMenuItemSpec>& items,
                                                     BitmapFont*                            font,
                                                     const EditorTheme&                     theme = DefaultEditorTheme)
    {
        return createEditorMenu(ui, parent, rect, items, font, theme);
    }

    inline EditorWidgetHandles createEditorAssetPicker(UiSystem&                           ui,
                                                       UiHandle                            parent,
                                                       const ToolRect&                     rect,
                                                       const std::vector<EditorAssetSpec>& assets,
                                                       BitmapFont*                         font,
                                                       std::string_view                    query = {},
                                                       const EditorTheme&                  theme = DefaultEditorTheme)
    {
        EditorWidgetHandles handles;
        handles.root = createRectRelative(ui, parent, rect, theme.colors.panelBackground);
        EditorWidgetHandles search =
            createEditorSearchField(ui, handles.root, {0.025f, 0.030f, 0.950f, 0.080f}, "SEARCH", query, font, theme);
        handles.fields.insert(handles.fields.end(), search.fields.begin(), search.fields.end());

        float y = 0.135f;
        for (const EditorAssetSpec& asset : assets)
        {
            if (!asset.visible)
                continue;

            const UiColor rowColor = asset.selected ? theme.colors.selection : theme.colors.panelInset;
            UiHandle      row =
                createRectRelative(ui, handles.root, {0.025f, y, 0.950f, theme.dimensions.rowHeight}, rowColor, true);
            UiHandle name = createTextRelative(ui,
                                               row,
                                               {0.025f, 0.12f, 0.62f, 0.76f},
                                               font,
                                               asset.name,
                                               theme.colors.text,
                                               theme.typography.smallScale);
            UiHandle type = createTextRelative(ui,
                                               row,
                                               {0.700f, 0.12f, 0.275f, 0.76f},
                                               font,
                                               asset.type,
                                               theme.colors.mutedText,
                                               theme.typography.smallScale);
            setTextAlignment(ui, type, UiHorizontalAlign::Right, UiVerticalAlign::Middle);
            handles.surfaces.push_back(row);
            handles.labels.push_back(name);
            handles.labels.push_back(type);
            y += theme.dimensions.rowHeight + theme.spacing.xs;
        }
        return handles;
    }

    inline EditorWidgetHandles createEditorNumericDrag(UiSystem&          ui,
                                                       UiHandle           parent,
                                                       const ToolRect&    rect,
                                                       std::string_view   label,
                                                       float              minValue,
                                                       float              maxValue,
                                                       float              value,
                                                       BitmapFont*        font,
                                                       bool               wholeNumber = false,
                                                       const EditorTheme& theme       = DefaultEditorTheme)
    {
        EditorWidgetHandles handles;
        handles.root = createContainerRelative(ui, parent, rect);
        ToolSlider slider =
            createSlider(ui, handles.root, 0.0f, std::string(label), minValue, maxValue, wholeNumber, font);
        updateSlider(ui, slider, value, theme.colors.accent);
        handles.sliders.push_back(slider);
        return handles;
    }

    inline EditorWidgetHandles createEditorDualRangeSlider(UiSystem&          ui,
                                                           UiHandle           parent,
                                                           const ToolRect&    rect,
                                                           std::string_view   label,
                                                           float              minValue,
                                                           float              maxValue,
                                                           float              lowValue,
                                                           float              highValue,
                                                           BitmapFont*        font,
                                                           const EditorTheme& theme = DefaultEditorTheme)
    {
        EditorWidgetHandles handles;
        handles.root = createRectRelative(ui, parent, rect, theme.colors.panelInset);
        handles.labels.push_back(createTextRelative(ui,
                                                    handles.root,
                                                    {0.0f, 0.0f, 0.30f, 1.0f},
                                                    font,
                                                    std::string(label),
                                                    theme.colors.mutedText,
                                                    theme.typography.smallScale));
        const float lowT =
            maxValue <= minValue ? 0.0f : std::clamp((lowValue - minValue) / (maxValue - minValue), 0.0f, 1.0f);
        const float highT =
            maxValue <= minValue ? 0.0f : std::clamp((highValue - minValue) / (maxValue - minValue), 0.0f, 1.0f);
        handles.surfaces.push_back(
            createRectRelative(ui, handles.root, {0.34f, 0.42f, 0.46f, 0.16f}, theme.colors.sliderTrack, true));
        handles.surfaces.push_back(
            createRectRelative(ui,
                               handles.root,
                               {0.34f + 0.46f * lowT, 0.35f, std::max(0.01f, 0.46f * (highT - lowT)), 0.30f},
                               theme.colors.accent,
                               true));
        return handles;
    }

    inline EditorWidgetHandles createEditorEnumDropdown(UiSystem&          ui,
                                                        UiHandle           parent,
                                                        const ToolRect&    rect,
                                                        std::string_view   label,
                                                        std::string_view   selected,
                                                        BitmapFont*        font,
                                                        const EditorTheme& theme = DefaultEditorTheme)
    {
        EditorWidgetHandles handles;
        handles.root = createRectRelative(ui, parent, rect, theme.colors.panelInset);
        handles.labels.push_back(createTextRelative(ui,
                                                    handles.root,
                                                    {0.025f, 0.12f, 0.38f, 0.76f},
                                                    font,
                                                    std::string(label),
                                                    theme.colors.mutedText,
                                                    theme.typography.smallScale));
        handles.buttons.push_back(createButtonRelative(ui,
                                                       handles.root,
                                                       {0.430f, 0.09f, 0.545f, 0.82f},
                                                       font,
                                                       std::string(selected),
                                                       theme.typography.smallScale));
        return handles;
    }

    inline EditorWidgetHandles createEditorTagSelector(UiSystem&                       ui,
                                                       UiHandle                        parent,
                                                       const ToolRect&                 rect,
                                                       const std::vector<std::string>& tags,
                                                       BitmapFont*                     font,
                                                       const EditorTheme&              theme = DefaultEditorTheme)
    {
        EditorWidgetHandles handles;
        handles.root = createRectRelative(ui, parent, rect, theme.colors.panelInset);
        if (tags.empty())
            return handles;

        const float gap = theme.spacing.xs;
        const float width =
            std::max(0.04f, (1.0f - gap * static_cast<float>(tags.size() - 1)) / static_cast<float>(tags.size()));
        for (size_t i = 0; i < tags.size(); ++i)
        {
            const float x = static_cast<float>(i) * (width + gap);
            handles.buttons.push_back(createButtonRelative(
                ui, handles.root, {x, 0.12f, width, 0.76f}, font, tags[i], theme.typography.smallScale));
        }
        return handles;
    }

    inline EditorWidgetHandles createEditorColorPicker(UiSystem&          ui,
                                                       UiHandle           parent,
                                                       const ToolRect&    rect,
                                                       UiColor            color,
                                                       BitmapFont*        font,
                                                       const EditorTheme& theme = DefaultEditorTheme)
    {
        EditorWidgetHandles handles;
        handles.root = createRectRelative(ui, parent, rect, theme.colors.panelInset);
        handles.surfaces.push_back(createRectRelative(ui, handles.root, {0.025f, 0.18f, 0.180f, 0.64f}, color, true));
        handles.sliders.push_back(createSlider(ui, handles.root, 0.10f, "R", 0.0f, 1.0f, false, font));
        handles.sliders.push_back(createSlider(ui, handles.root, 0.40f, "G", 0.0f, 1.0f, false, font));
        handles.sliders.push_back(createSlider(ui, handles.root, 0.70f, "B", 0.0f, 1.0f, false, font));
        updateSlider(ui, handles.sliders[0], color.r, theme.colors.error);
        updateSlider(ui, handles.sliders[1], color.g, theme.colors.success);
        updateSlider(ui, handles.sliders[2], color.b, theme.colors.axisZ);
        return handles;
    }

    inline EditorWidgetHandles createEditorGradientEditor(UiSystem&                                 ui,
                                                          UiHandle                                  parent,
                                                          const ToolRect&                           rect,
                                                          const std::vector<EditorGradientKeySpec>& keys,
                                                          const EditorTheme& theme = DefaultEditorTheme)
    {
        EditorWidgetHandles handles;
        handles.root = createRectRelative(ui, parent, rect, theme.colors.panelBackground);
        handles.surfaces.push_back(
            createRectRelative(ui, handles.root, {0.025f, 0.38f, 0.950f, 0.24f}, theme.colors.sliderTrack, true));
        for (const EditorGradientKeySpec& key : keys)
        {
            const float x = 0.025f + std::clamp(key.position, 0.0f, 1.0f) * 0.900f;
            handles.surfaces.push_back(createRectRelative(
                ui, handles.root, {x, key.selected ? 0.18f : 0.25f, 0.045f, 0.50f}, key.color, true));
        }
        return handles;
    }

    inline EditorWidgetHandles createEditorCurveEditor(UiSystem&                              ui,
                                                       UiHandle                               parent,
                                                       const ToolRect&                        rect,
                                                       const std::vector<EditorCurveKeySpec>& keys,
                                                       const EditorTheme& theme = DefaultEditorTheme)
    {
        EditorWidgetHandles handles;
        handles.root = createRectRelative(ui, parent, rect, theme.colors.graphBackground);
        for (int i = 1; i < 4; ++i)
        {
            const float t = static_cast<float>(i) * 0.25f;
            handles.surfaces.push_back(
                createEditorLineRelative(ui, handles.root, {t, 0.0f}, {t, 1.0f}, theme.colors.graphGridMajor, 0.001f));
            handles.surfaces.push_back(
                createEditorLineRelative(ui, handles.root, {0.0f, t}, {1.0f, t}, theme.colors.graphGridMajor, 0.001f));
        }
        for (const EditorCurveKeySpec& key : keys)
        {
            const float x = std::clamp(key.time, 0.0f, 1.0f);
            const float y = 1.0f - std::clamp(key.value, 0.0f, 1.0f);
            handles.surfaces.push_back(
                createEditorCircleRelative(ui,
                                           handles.root,
                                           {x - 0.012f, y - 0.012f, 0.024f, 0.024f},
                                           key.selected ? theme.colors.selection : theme.colors.accent,
                                           true));
        }
        return handles;
    }

    inline EditorWidgetHandles createEditorTimeline(UiSystem&                                   ui,
                                                    UiHandle                                    parent,
                                                    const ToolRect&                             rect,
                                                    const std::vector<EditorTimelineTrackSpec>& tracks,
                                                    BitmapFont*                                 font,
                                                    const EditorTheme& theme = DefaultEditorTheme)
    {
        EditorWidgetHandles handles;
        handles.root          = createRectRelative(ui, parent, rect, theme.colors.graphBackground);
        const float rowHeight = tracks.empty() ? 1.0f : std::min(0.18f, 0.92f / static_cast<float>(tracks.size()));
        float       y         = 0.04f;
        for (const EditorTimelineTrackSpec& track : tracks)
        {
            UiHandle row = createRectRelative(ui, handles.root, {0.0f, y, 1.0f, rowHeight}, theme.colors.timelineTrack);
            handles.labels.push_back(createTextRelative(ui,
                                                        row,
                                                        {0.025f, 0.12f, 0.20f, 0.76f},
                                                        font,
                                                        track.label,
                                                        theme.colors.text,
                                                        theme.typography.smallScale));
            for (float key : track.keys)
            {
                const float x = 0.260f + std::clamp(key, 0.0f, 1.0f) * 0.700f;
                handles.surfaces.push_back(
                    createRectRelative(ui, row, {x, 0.22f, 0.010f, 0.56f}, theme.colors.accent, true));
            }
            handles.surfaces.push_back(row);
            y += rowHeight + theme.spacing.xs;
        }
        return handles;
    }

    inline EditorWidgetHandles createEditorGraphCanvas(UiSystem&                               ui,
                                                       UiHandle                                parent,
                                                       const ToolRect&                         rect,
                                                       const std::vector<EditorGraphNodeSpec>& nodes,
                                                       const std::vector<EditorGraphLinkSpec>& links,
                                                       BitmapFont*                             font,
                                                       const EditorTheme& theme = DefaultEditorTheme)
    {
        EditorWidgetHandles handles;
        handles.root = createRectRelative(ui, parent, rect, theme.colors.graphBackground);
        for (int i = 1; i < 6; ++i)
        {
            const float t = static_cast<float>(i) / 6.0f;
            createEditorLineRelative(ui, handles.root, {t, 0.0f}, {t, 1.0f}, theme.colors.graphGridMinor, 0.001f);
            createEditorLineRelative(ui, handles.root, {0.0f, t}, {1.0f, t}, theme.colors.graphGridMinor, 0.001f);
        }
        for (const EditorGraphLinkSpec& link : links)
        {
            if (link.fromIndex >= nodes.size() || link.toIndex >= nodes.size())
                continue;
            const UiRect& from = nodes[link.fromIndex].rect;
            const UiRect& to   = nodes[link.toIndex].rect;
            handles.surfaces.push_back(createEditorLineRelative(ui,
                                                                handles.root,
                                                                {from.x + from.width, from.y + from.height * 0.5f},
                                                                {to.x, to.y + to.height * 0.5f},
                                                                theme.colors.border,
                                                                0.002f));
        }
        for (const EditorGraphNodeSpec& node : nodes)
        {
            UiColor  nodeColor = node.selected ? theme.colors.selection : theme.colors.panelSurfaceRaised;
            UiHandle surface   = createRectRelative(ui, handles.root, toToolRect(node.rect), nodeColor, true);
            UiHandle label     = createTextRelative(ui,
                                                    surface,
                                                    {0.05f, 0.20f, 0.90f, 0.60f},
                                                    font,
                                                    node.label,
                                                    theme.colors.text,
                                                    theme.typography.smallScale);
            setTextAlignment(ui, label, UiHorizontalAlign::Center, UiVerticalAlign::Middle);
            handles.surfaces.push_back(surface);
            handles.labels.push_back(label);
        }
        return handles;
    }

    inline EditorWidgetHandles createEditorSearchPalette(UiSystem&                              ui,
                                                         UiHandle                               parent,
                                                         const ToolRect&                        rect,
                                                         std::string_view                       query,
                                                         const std::vector<EditorMenuItemSpec>& results,
                                                         BitmapFont*                            font,
                                                         const EditorTheme& theme = DefaultEditorTheme)
    {
        EditorWidgetHandles handles;
        handles.root = createRectRelative(ui, parent, rect, theme.colors.overlay);
        EditorWidgetHandles search =
            createEditorSearchField(ui, handles.root, {0.025f, 0.035f, 0.950f, 0.100f}, "FIND", query, font, theme);
        EditorWidgetHandles menu =
            createEditorMenu(ui, handles.root, {0.025f, 0.165f, 0.950f, 0.800f}, results, font, theme);
        handles.fields.insert(handles.fields.end(), search.fields.begin(), search.fields.end());
        handles.buttons.insert(handles.buttons.end(), menu.buttons.begin(), menu.buttons.end());
        handles.surfaces.push_back(search.root);
        handles.surfaces.push_back(menu.root);
        return handles;
    }

    inline EditorWidgetHandles createEditorValidationPanel(UiSystem&                                       ui,
                                                           UiHandle                                        parent,
                                                           const ToolRect&                                 rect,
                                                           const std::vector<EditorValidationMessageSpec>& messages,
                                                           BitmapFont*                                     font,
                                                           const EditorTheme& theme = DefaultEditorTheme)
    {
        EditorWidgetHandles handles;
        handles.root = createRectRelative(ui, parent, rect, theme.colors.panelInset);
        float y      = theme.spacing.sm;
        for (const EditorValidationMessageSpec& message : messages)
        {
            UiColor  color = validationColor(message.severity, theme);
            UiHandle row   = createRectRelative(
                ui, handles.root, {0.012f, y, 0.976f, theme.dimensions.compactRowHeight}, theme.colors.cardBackground);
            handles.surfaces.push_back(createRectRelative(ui, row, {0.0f, 0.0f, 0.012f, 1.0f}, color));
            handles.labels.push_back(createTextRelative(
                ui, row, {0.030f, 0.12f, 0.240f, 0.76f}, font, message.source, color, theme.typography.smallScale));
            handles.labels.push_back(createTextRelative(ui,
                                                        row,
                                                        {0.290f, 0.12f, 0.685f, 0.76f},
                                                        font,
                                                        message.message,
                                                        theme.colors.text,
                                                        theme.typography.smallScale));
            handles.surfaces.push_back(row);
            y += theme.dimensions.compactRowHeight + theme.spacing.xs;
        }
        return handles;
    }

    inline EditorWidgetHandles createEditorConsoleOutput(UiSystem&                       ui,
                                                         UiHandle                        parent,
                                                         const ToolRect&                 rect,
                                                         const std::vector<std::string>& lines,
                                                         BitmapFont*                     font,
                                                         const EditorTheme&              theme = DefaultEditorTheme)
    {
        EditorWidgetHandles handles;
        handles.root = createRectRelative(ui, parent, rect, theme.colors.graphBackground);
        float y      = theme.spacing.xs;
        for (const std::string& line : lines)
        {
            handles.labels.push_back(createTextRelative(ui,
                                                        handles.root,
                                                        {0.025f, y, 0.950f, theme.dimensions.compactRowHeight},
                                                        font,
                                                        line,
                                                        theme.colors.statusText,
                                                        theme.typography.smallScale));
            y += theme.dimensions.compactRowHeight;
        }
        return handles;
    }

    inline EditorWidgetHandles createEditorDiagnosticsPanel(UiSystem&                                       ui,
                                                            UiHandle                                        parent,
                                                            const ToolRect&                                 rect,
                                                            const std::vector<EditorValidationMessageSpec>& messages,
                                                            BitmapFont*                                     font,
                                                            const EditorTheme& theme = DefaultEditorTheme)
    {
        return createEditorValidationPanel(ui, parent, rect, messages, font, theme);
    }
} // namespace gts::tools
