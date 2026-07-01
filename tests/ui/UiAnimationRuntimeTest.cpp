#include <cmath>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>

#include "UiSystem.h"
#include "UiWidget.h"

namespace
{
    void require(bool condition, const std::string& message)
    {
        if (!condition)
        {
            std::cerr << "UI animation runtime test failed: " << message << std::endl;
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

    UiAnimationTiming linearTiming(float duration = 1.0f)
    {
        UiAnimationTiming timing;
        timing.durationSeconds = duration;
        timing.ease = gts::tween::TweenEase::Linear;
        return timing;
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

    UiLayoutSpec box(float x, float y, float width, float height)
    {
        UiLayoutSpec layout;
        layout.positionMode = UiPositionMode::Absolute;
        layout.widthMode = UiSizeMode::Fixed;
        layout.heightMode = UiSizeMode::Fixed;
        layout.offsetMin = {x, y};
        layout.fixedWidth = width;
        layout.fixedHeight = height;
        return layout;
    }

    UiHandle createRect(UiSystem& ui, UiHandle parent, const UiLayoutSpec& layout)
    {
        const UiHandle handle = ui.createNode(UiNodeType::Rect, parent);
        ui.setLayout(handle, layout);
        ui.setPayload(handle, UiRectData{{1.0f, 1.0f, 1.0f, 1.0f}});
        ui.setState(handle, UiStateFlags{.visible = true, .enabled = true, .interactable = true});
        return handle;
    }

    UiHandle createRect(UiSystem& ui, UiSurfaceId surface, UiHandle parent, const UiLayoutSpec& layout)
    {
        const UiHandle handle = ui.createNode(surface, UiNodeType::Rect, parent);
        ui.setLayout(surface, handle, layout);
        ui.setPayload(surface, handle, UiRectData{{1.0f, 1.0f, 1.0f, 1.0f}});
        ui.setState(surface, handle, UiStateFlags{.visible = true, .enabled = true, .interactable = true});
        return handle;
    }

    void extract(UiSystem& ui)
    {
        ui.extractCommandsRef(800, 600);
    }

    void extract(UiSystem& ui, UiSurfaceId surface)
    {
        ui.extractSurfaceCommandsRef(surface, 800, 600);
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

    const UiRect& bounds(UiSystem& ui, UiHandle handle)
    {
        const UiNode* node = ui.findNode(handle);
        require(node != nullptr, "node missing");
        return node->computedLayout.bounds;
    }

    UiInputFrame pointerFrame(float x, float y)
    {
        UiInputFrame frame;
        frame.pointerX = x;
        frame.pointerY = y;
        return frame;
    }

    struct ButtonAnimationState
    {
        UiHandle button = UI_INVALID_HANDLE;
    };

    class ButtonAnimationComposition : public UiComposition
    {
    public:
        explicit ButtonAnimationComposition(ButtonAnimationState& inState)
            : state(inState)
        {
        }

        void build(UiCompositionContext& context) override
        {
            context.ui.setLayout(context.root, fillLayout());
            gts::ui::UiWidgetContext widgetContext(context);

            gts::ui::UiButtonDesc desc;
            desc.layout = box(0.10f, 0.10f, 0.30f, 0.16f);
            desc.text = "Animated";
            desc.stateAnimation = UiStyleTransitionDesc{linearTiming()};
            button.build(widgetContext, context.root, desc);
            state.button = button.root();
        }

        void onEvent(UiCompositionContext& context, UiEvent& event) override
        {
            gts::ui::UiWidgetContext widgetContext(context);
            button.onEvent(widgetContext, event);
        }

        void destroy(UiCompositionContext& context) override
        {
            gts::ui::UiWidgetContext widgetContext(context);
            button.destroy(widgetContext);
        }

    private:
        ButtonAnimationState& state;
        gts::ui::UiButtonWidget button;
    };

    void testPropertyInterpolationAndCompletion()
    {
        UiSystem ui(nullptr);
        const UiHandle rect = createRect(ui, ui.getRoot(), box(0.0f, 0.0f, 0.20f, 0.20f));

        UiAnimationDesc animation;
        animation.target = rect;
        animation.property = UiAnimationProperty::Opacity;
        animation.from = 0.0f;
        animation.to = 1.0f;
        animation.timing = linearTiming();
        const UiAnimationId id = ui.animate(animation);
        require(id != UI_INVALID_ANIMATION, "opacity animation was not created");

        ui.updateAnimations(0.5f);
        extract(ui);
        const UiRectPrimitive* primitive = findRectPrimitive(ui.getDocument(), rect);
        require(primitive != nullptr, "animated rect primitive missing");
        require(near(primitive->color.a, 0.5f), "opacity did not interpolate halfway");

        const UiAnimationFrameResult result = ui.updateAnimations(0.5f);
        extract(ui);
        primitive = findRectPrimitive(ui.getDocument(), rect);
        require(primitive != nullptr && near(primitive->color.a, 1.0f), "opacity did not complete");
        require(result.completed == 1, "completion was not reported");
        require(!ui.isAnimating(rect), "completed animation remained active");
    }

    void testInterruptionAndCancellation()
    {
        UiSystem ui(nullptr);
        const UiHandle rect = createRect(ui, ui.getRoot(), box(0.0f, 0.0f, 0.20f, 0.20f));

        UiAnimationDesc up;
        up.target = rect;
        up.property = UiAnimationProperty::Opacity;
        up.from = 0.0f;
        up.to = 1.0f;
        up.timing = linearTiming();
        ui.animate(up);
        ui.updateAnimations(0.4f);

        ui.animateOpacity(rect, 0.0f, linearTiming());
        ui.updateAnimations(0.5f);
        extract(ui);
        const UiRectPrimitive* primitive = findRectPrimitive(ui.getDocument(), rect);
        require(primitive != nullptr, "interrupted rect primitive missing");
        require(near(primitive->color.a, 0.2f), "interrupted animation did not start from current value");

        require(ui.cancelAnimations(rect, true) == 1, "animation was not cancelled");
        extract(ui);
        primitive = findRectPrimitive(ui.getDocument(), rect);
        require(primitive != nullptr && near(primitive->color.a, 0.0f), "complete cancel did not snap to target");
    }

    void testLayoutTransition()
    {
        UiSystem ui(nullptr);
        const UiHandle rect = createRect(ui, ui.getRoot(), box(0.0f, 0.0f, 0.20f, 0.20f));

        UiAnimationDesc animation;
        animation.target = rect;
        animation.property = UiAnimationProperty::LayoutOffsetMin;
        animation.from = UiVec2{0.0f, 0.0f};
        animation.to = UiVec2{0.50f, 0.25f};
        animation.timing = linearTiming();
        ui.animate(animation);
        ui.updateAnimations(0.5f);
        extract(ui);

        require(near(bounds(ui, rect).x, 0.25f), "layout x did not interpolate");
        require(near(bounds(ui, rect).y, 0.125f), "layout y did not interpolate");
    }

    void testThemeDrivenButtonHoverTransition()
    {
        UiSystem ui(nullptr);
        UiTheme theme = defaultUiTheme();
        theme.setColor("Normal", {0.10f, 0.20f, 0.30f, 1.0f});
        theme.setColor("Hover", {0.50f, 0.60f, 0.70f, 1.0f});

        UiStyleClass button;
        button.base.backgroundColorToken = "Normal";
        button.states[UiStyleState::Hover].backgroundColorToken = "Hover";
        theme.setStyleClass("Button", button);
        ui.setTheme(theme);

        ButtonAnimationState state;
        ui.mountComposition(std::make_unique<ButtonAnimationComposition>(state));
        extract(ui);

        ui.dispatchInput(pointerFrame(0.15f, 0.15f), 1);
        require(ui.isAnimating(state.button, UiAnimationProperty::BackgroundColor),
                "button hover did not start style animation");

        ui.updateAnimations(0.5f);
        extract(ui);
        const UiRectPrimitive* primitive = findRectPrimitive(ui.getDocument(), state.button);
        require(primitive != nullptr, "button primitive missing");
        require(nearColor(primitive->color, {0.30f, 0.40f, 0.50f, 1.0f}),
                "theme-driven hover color did not interpolate");
    }

    void testMountDestructionCancelsAnimations()
    {
        UiSystem ui(nullptr);
        const UiMountId mount = ui.createMount();
        const UiHandle root = ui.mountRoot(mount);
        const UiHandle rect = createRect(ui, root, box(0.0f, 0.0f, 0.20f, 0.20f));
        ui.animateOpacity(rect, 0.0f, linearTiming());
        require(ui.isAnimating(rect), "mount test animation was not active");

        require(ui.destroyMount(mount), "mount destroy failed");
        require(!ui.animationManager().active(), "animation survived mount destruction");
    }

    void testModalOpenCloseTransitionHooks()
    {
        UiSystem ui(nullptr);
        const UiLayerId modalLayer = ui.createLayer("modal", 10);
        const UiHandle owner = createRect(ui, ui.getLayerRoot(modalLayer), box(0.25f, 0.25f, 0.30f, 0.30f));

        UiModalDesc modal;
        modal.layer = modalLayer;
        modal.owner = owner;
        modal.openAnimation = linearTiming();
        modal.closeAnimation = linearTiming();
        const UiModalId modalId = ui.pushModal(modal);
        require(modalId != UI_INVALID_MODAL, "modal push failed");
        require(ui.isAnimating(owner, UiAnimationProperty::Opacity), "modal open animation did not start");

        ui.updateAnimations(0.5f);
        extract(ui);
        const UiRectPrimitive* primitive = findRectPrimitive(ui.getDocument(), owner);
        require(primitive != nullptr && near(primitive->color.a, 0.5f), "modal open did not fade owner");

        require(ui.popModal(modalId), "modal pop failed");
        ui.updateAnimations(0.5f);
        extract(ui);
        primitive = findRectPrimitive(ui.getDocument(), owner);
        require(primitive != nullptr && near(primitive->color.a, 0.25f), "modal close did not interrupt from current opacity");
    }

    void testSurfaceLocalAnimation()
    {
        UiSystem ui(nullptr);
        UiSurfaceDesc desc;
        desc.name = "secondary";
        desc.order = 10;
        desc.rect = {0.5f, 0.0f, 0.5f, 1.0f};
        const UiSurfaceId surface = ui.createSurface(desc);

        const UiHandle defaultRect = createRect(ui, ui.getRoot(), box(0.0f, 0.0f, 0.20f, 0.20f));
        const UiHandle surfaceRect = createRect(ui, surface, ui.getRoot(surface), box(0.0f, 0.0f, 0.20f, 0.20f));
        ui.animateOpacity(surface, surfaceRect, 0.0f, linearTiming());
        require(!ui.isAnimating(defaultRect), "default surface incorrectly owns secondary animation");
        require(ui.isAnimating(surface, surfaceRect), "secondary surface animation was not active");

        ui.updateAnimations(0.5f);
        extract(ui, surface);
        const UiDocument* document = ui.findDocument(surface);
        require(document != nullptr, "secondary document missing");
        const UiRectPrimitive* primitive = findRectPrimitive(*document, surfaceRect);
        require(primitive != nullptr && near(primitive->color.a, 0.5f), "secondary animation did not update");
    }
}

int main()
{
    testPropertyInterpolationAndCompletion();
    testInterruptionAndCancellation();
    testLayoutTransition();
    testThemeDrivenButtonHoverTransition();
    testMountDestructionCancelsAnimations();
    testModalOpenCloseTransitionHooks();
    testSurfaceLocalAnimation();
    return 0;
}
