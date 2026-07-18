#pragma once

#include <string>

#include "ToolPaneBase.h"
#include "ToolPropertyInspector.h"

namespace gts::tools
{
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

} // namespace gts::tools
