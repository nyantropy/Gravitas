#include <cmath>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>

#include "UiSystem.h"
#include "UiWidget.h"

namespace
{
    struct WidgetProbeState
    {
        gts::ui::UiButtonWidget* button = nullptr;
        gts::ui::UiPanelWidget* panel = nullptr;
        gts::ui::UiLabelWidget* label = nullptr;
        gts::ui::UiProgressBarWidget* progress = nullptr;
        UiHandle separator = UI_INVALID_HANDLE;
        UiHandle stack = UI_INVALID_HANDLE;
        UiHandle buttonRoot = UI_INVALID_HANDLE;
        UiHandle buttonLabel = UI_INVALID_HANDLE;
        UiHandle progressRoot = UI_INVALID_HANDLE;
        UiHandle progressFill = UI_INVALID_HANDLE;
        int pressedCount = 0;
        bool disabledButton = false;
    };

    void require(bool condition, const std::string& message)
    {
        if (!condition)
        {
            std::cerr << "UI widget runtime test failed: " << message << std::endl;
            std::exit(1);
        }
    }

    bool near(float lhs, float rhs, float epsilon = 0.001f)
    {
        return std::fabs(lhs - rhs) <= epsilon;
    }

    bool nearColor(const UiColor& lhs, const UiColor& rhs, float epsilon = 0.001f)
    {
        return near(lhs.r, rhs.r, epsilon)
            && near(lhs.g, rhs.g, epsilon)
            && near(lhs.b, rhs.b, epsilon)
            && near(lhs.a, rhs.a, epsilon);
    }

    UiLayoutSpec fillLayout()
    {
        UiLayoutSpec layout;
        layout.positionMode = UiPositionMode::Anchored;
        layout.widthMode = UiSizeMode::FromAnchors;
        layout.heightMode = UiSizeMode::FromAnchors;
        layout.anchorMin = {0.0f, 0.0f};
        layout.anchorMax = {1.0f, 1.0f};
        return layout;
    }

    UiLayoutSpec fixedLayout(float width, float height)
    {
        UiLayoutSpec layout;
        layout.constraints.preferredWidth = {UiLayoutUnit::Normalized, width};
        layout.constraints.preferredHeight = {UiLayoutUnit::Normalized, height};
        layout.constraints.horizontalAlignment = UiLayoutAlignment::Start;
        layout.constraints.verticalAlignment = UiLayoutAlignment::Start;
        return layout;
    }

    const UiRect& bounds(UiSystem& ui, UiHandle handle)
    {
        const UiNode* node = ui.findNode(handle);
        require(node != nullptr, "node missing");
        return node->computedLayout.bounds;
    }

    const UiRectPrimitive* findRectPrimitive(const UiDocument& document, UiHandle handle)
    {
        for (const UiVisualPrimitive& primitive : document.getVisualList().primitives)
        {
            if (const auto* rect = std::get_if<UiRectPrimitive>(&primitive))
            {
                if (rect->source == handle)
                    return rect;
            }
        }
        return nullptr;
    }

    const UiNineSlicePrimitive* findNineSlicePrimitive(const UiDocument& document, UiHandle handle)
    {
        for (const UiVisualPrimitive& primitive : document.getVisualList().primitives)
        {
            if (const auto* nineSlice = std::get_if<UiNineSlicePrimitive>(&primitive))
            {
                if (nineSlice->source == handle)
                    return nineSlice;
            }
        }
        return nullptr;
    }

    const UiTextPrimitive* findTextPrimitive(const UiDocument& document, UiHandle handle)
    {
        for (const UiVisualPrimitive& primitive : document.getVisualList().primitives)
        {
            if (const auto* text = std::get_if<UiTextPrimitive>(&primitive))
            {
                if (text->source == handle)
                    return text;
            }
        }
        return nullptr;
    }

    UiInputFrame pointerFrame(float x, float y)
    {
        UiInputFrame frame;
        frame.pointerX = x;
        frame.pointerY = y;
        return frame;
    }

    UiInputFrame pressFrame(float x, float y)
    {
        UiInputFrame frame = pointerFrame(x, y);
        frame.primaryDown = true;
        frame.primaryPressed = true;
        return frame;
    }

    UiInputFrame releaseFrame(float x, float y)
    {
        UiInputFrame frame = pointerFrame(x, y);
        frame.primaryReleased = true;
        return frame;
    }

    void extract(UiSystem& ui)
    {
        ui.extractCommandsRef(800, 600);
    }

    class WidgetProbeComposition : public UiComposition
    {
    public:
        explicit WidgetProbeComposition(WidgetProbeState& inState)
            : state(inState)
        {
        }

        void build(UiCompositionContext& context) override
        {
            gts::ui::UiWidgetContext widgetContext(context);

            UiLayoutSpec rootLayout = fillLayout();
            rootLayout.layoutMode = UiLayoutMode::Overlay;
            context.ui.setLayout(context.root, rootLayout);

            gts::ui::UiPanelDesc panelDesc;
            panelDesc.layout = fillLayout();
            panelDesc.styleClass = "Panel";
            panel.build(widgetContext, context.root, panelDesc);

            gts::ui::UiStackDesc stackDesc;
            stackDesc.layout = fillLayout();
            stackDesc.axis = UiLayoutAxis::Vertical;
            stackDesc.gap = 0.05f;
            stack.build(widgetContext, panel.content(), stackDesc);
            state.stack = stack.root();

            gts::ui::UiLabelDesc labelDesc;
            labelDesc.layout = fixedLayout(0.0f, 0.10f);
            labelDesc.text = "Widget label";
            labelDesc.styleClass = "Text.Body";
            label.build(widgetContext, stack.root(), labelDesc);

            gts::ui::UiButtonDesc buttonDesc;
            buttonDesc.layout = fixedLayout(0.60f, 0.20f);
            buttonDesc.text = "Press";
            buttonDesc.enabled = !state.disabledButton;
            buttonDesc.onPressed = [&state = state]()
            {
                ++state.pressedCount;
            };
            button.build(widgetContext, stack.root(), buttonDesc);

            gts::ui::UiSeparatorDesc separatorDesc;
            separatorDesc.layout = fixedLayout(0.0f, 0.02f);
            separator.build(widgetContext, stack.root(), separatorDesc);

            gts::ui::UiProgressBarDesc progressDesc;
            progressDesc.layout = fixedLayout(0.70f, 0.08f);
            progressDesc.value = 0.40f;
            progress.build(widgetContext, stack.root(), progressDesc);

            state.button = &button;
            state.panel = &panel;
            state.label = &label;
            state.progress = &progress;
            state.separator = separator.root();
            state.buttonRoot = button.root();
            state.buttonLabel = button.label();
            state.progressRoot = progress.root();
            state.progressFill = progress.fill();
        }

        void onEvent(UiCompositionContext& context, UiEvent& event) override
        {
            gts::ui::UiWidgetContext widgetContext(context);
            button.onEvent(widgetContext, event);
        }

        void destroy(UiCompositionContext& context) override
        {
            gts::ui::UiWidgetContext widgetContext(context);
            progress.destroy(widgetContext);
            separator.destroy(widgetContext);
            button.destroy(widgetContext);
            label.destroy(widgetContext);
            stack.destroy(widgetContext);
            panel.destroy(widgetContext);

            state.button = nullptr;
            state.panel = nullptr;
            state.label = nullptr;
            state.progress = nullptr;
        }

    private:
        WidgetProbeState& state;
        gts::ui::UiPanelWidget panel;
        gts::ui::UiStackWidget stack;
        gts::ui::UiLabelWidget label;
        gts::ui::UiButtonWidget button;
        gts::ui::UiSeparatorWidget separator;
        gts::ui::UiProgressBarWidget progress;
    };

    void dispatchClick(UiSystem& ui, float x, float y)
    {
        ui.dispatchInput(pointerFrame(x, y), 1);
        ui.dispatchInput(pressFrame(x, y), 2);
        ui.dispatchInput(releaseFrame(x, y), 3);
    }

    void testWidgetCreationThemeAndLayout()
    {
        UiSystem ui(nullptr);

        UiTheme theme = defaultUiTheme();
        theme.setColor("ButtonNormal", {0.2f, 0.3f, 0.4f, 1.0f});
        theme.setColor("SeparatorColor", {0.8f, 0.1f, 0.2f, 1.0f});

        UiStyleClass buttonStyle;
        buttonStyle.base.backgroundColorToken = "ButtonNormal";
        theme.setStyleClass("Button", buttonStyle);

        UiStyleClass separatorStyle;
        separatorStyle.base.backgroundColorToken = "SeparatorColor";
        theme.setStyleClass("Separator", separatorStyle);
        ui.setTheme(theme);

        WidgetProbeState state;
        ui.mountComposition(std::make_unique<WidgetProbeComposition>(state));
        extract(ui);

        require(ui.findNode(state.buttonRoot) != nullptr, "button root was not created");
        require(ui.findNode(state.buttonLabel) != nullptr, "button label was not created");
        require(ui.findNode(state.progressRoot) != nullptr, "progress root was not created");
        require(ui.findNode(state.progressFill) != nullptr, "progress fill was not created");
        require(near(bounds(ui, state.buttonRoot).y, 0.15f), "button stack position");
        require(near(bounds(ui, state.progressRoot).y, 0.47f), "progress stack position");
        require(near(bounds(ui, state.progressFill).width, bounds(ui, state.progressRoot).width * 0.40f),
                "progress fill width");

        const UiRectPrimitive* buttonRect = findRectPrimitive(ui.getDocument(), state.buttonRoot);
        const UiRectPrimitive* separatorRect = findRectPrimitive(ui.getDocument(), state.separator);
        const UiTextPrimitive* buttonText = findTextPrimitive(ui.getDocument(), state.buttonLabel);
        require(buttonRect != nullptr, "button rect primitive missing");
        require(separatorRect != nullptr, "separator rect primitive missing");
        require(buttonText != nullptr, "button text primitive missing");
        require(nearColor(buttonRect->color, {0.2f, 0.3f, 0.4f, 1.0f}), "button theme style not applied");
        require(nearColor(separatorRect->color, {0.8f, 0.1f, 0.2f, 1.0f}), "separator theme style not applied");
    }

    void testWidgetRectStyleCanRenderNineSliceSkin()
    {
        UiSystem ui(nullptr);

        UiTheme theme = defaultUiTheme();
        UiPanelSkin skin;
        skin.type = UiPanelSkinType::NineSlice;
        skin.imageAsset = "resources/textures/ui/yune_choice_panel.png";
        skin.tint = {0.8f, 0.9f, 1.0f, 1.0f};
        skin.slice = {0.15f, 0.15f, 0.15f, 0.15f};

        UiStyleClass buttonStyle;
        buttonStyle.base.skin = skin;
        theme.setStyleClass("Button", buttonStyle);
        ui.setTheme(theme);

        WidgetProbeState state;
        ui.mountComposition(std::make_unique<WidgetProbeComposition>(state));
        extract(ui);

        const UiNineSlicePrimitive* buttonSkin = findNineSlicePrimitive(ui.getDocument(), state.buttonRoot);
        require(buttonSkin != nullptr, "button widget did not render nine-slice skin");
        require(buttonSkin->imageAsset == skin.imageAsset, "button nine-slice asset was not resolved from style");
        require(findRectPrimitive(ui.getDocument(), state.buttonRoot) == nullptr,
                "button rendered fallback rect instead of nine-slice skin");
    }

    void testButtonClickThroughEventPropagation()
    {
        UiSystem ui(nullptr);
        WidgetProbeState state;
        ui.mountComposition(std::make_unique<WidgetProbeComposition>(state));
        extract(ui);

        const UiRect buttonBounds = bounds(ui, state.buttonRoot);
        const float x = buttonBounds.x + buttonBounds.width * 0.5f;
        const float y = buttonBounds.y + buttonBounds.height * 0.5f;

        ui.dispatchInput(pointerFrame(x, y), 1);
        const UiNode* buttonNode = ui.findNode(state.buttonRoot);
        require(buttonNode != nullptr && buttonNode->state.hovered, "button did not reflect hover state");

        ui.dispatchInput(pressFrame(x, y), 2);
        buttonNode = ui.findNode(state.buttonRoot);
        require(buttonNode != nullptr && buttonNode->state.pressed, "button did not reflect pressed state");

        ui.dispatchInput(releaseFrame(x, y), 3);

        require(state.pressedCount == 1, "button did not emit semantic pressed callback");
        buttonNode = ui.findNode(state.buttonRoot);
        require(buttonNode != nullptr && buttonNode->state.focused, "button did not reflect focus state");

        const auto& events = ui.events();
        require(!events.empty(), "dispatch did not publish events");
        require(events.back().type == UiEventType::PointerClick, "last event was not pointer click");
        require(events.back().consumed, "button click event was not consumed");
        require(events.back().defaultPrevented, "button click default was not prevented");
    }

    void testDisabledButtonDoesNotPress()
    {
        UiSystem ui(nullptr);
        WidgetProbeState state;
        state.disabledButton = true;
        ui.mountComposition(std::make_unique<WidgetProbeComposition>(state));
        extract(ui);

        const UiRect buttonBounds = bounds(ui, state.buttonRoot);
        dispatchClick(ui, buttonBounds.x + buttonBounds.width * 0.5f, buttonBounds.y + buttonBounds.height * 0.5f);

        require(state.pressedCount == 0, "disabled button emitted pressed callback");
        const UiNode* buttonNode = ui.findNode(state.buttonRoot);
        require(buttonNode != nullptr && !buttonNode->state.enabled, "disabled button state was not retained");
    }

    void testCompositionCleanupDestroysWidgetSubtrees()
    {
        UiSystem ui(nullptr);
        WidgetProbeState state;
        const UiCompositionId composition =
            ui.mountComposition(std::make_unique<WidgetProbeComposition>(state));

        const UiHandle buttonRoot = state.buttonRoot;
        const UiHandle progressFill = state.progressFill;
        require(ui.findNode(buttonRoot) != nullptr, "button root missing before cleanup");
        require(ui.findNode(progressFill) != nullptr, "progress fill missing before cleanup");

        require(ui.destroyComposition(composition), "destroy composition failed");
        require(ui.findNode(buttonRoot) == nullptr, "button root survived composition cleanup");
        require(ui.findNode(progressFill) == nullptr, "progress fill survived composition cleanup");
    }
} // namespace

int main()
{
    testWidgetCreationThemeAndLayout();
    testWidgetRectStyleCanRenderNineSliceSkin();
    testButtonClickThroughEventPropagation();
    testDisabledButtonDoesNotPress();
    testCompositionCleanupDestroysWidgetSubtrees();
    return 0;
}
