#include <cmath>
#include <algorithm>
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
            std::cerr << "UI binding runtime test failed: " << message << std::endl;
            std::exit(1);
        }
    }

    bool near(float lhs, float rhs, float epsilon = 0.001f)
    {
        return std::fabs(lhs - rhs) <= epsilon;
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

    UiLayoutSpec fixedLayout(float width, float height)
    {
        UiLayoutSpec layout;
        layout.constraints.preferredWidth = {UiLayoutUnit::Normalized, width};
        layout.constraints.preferredHeight = {UiLayoutUnit::Normalized, height};
        layout.constraints.horizontalAlignment = UiLayoutAlignment::Start;
        layout.constraints.verticalAlignment = UiLayoutAlignment::Start;
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

    UiHandle createText(UiSystem& ui, UiHandle parent)
    {
        const UiHandle handle = ui.createNode(UiNodeType::Text, parent);
        ui.setLayout(handle, box(0.0f, 0.0f, 0.4f, 0.1f));
        ui.setPayload(handle, UiTextData{"", {}, {1.0f, 1.0f, 1.0f, 1.0f}, 1.0f});
        return handle;
    }

    UiHandle createRect(UiSystem& ui, UiHandle parent)
    {
        const UiHandle handle = ui.createNode(UiNodeType::Rect, parent);
        ui.setLayout(handle, box(0.0f, 0.0f, 0.4f, 0.1f));
        ui.setPayload(handle, UiRectData{{1.0f, 1.0f, 1.0f, 1.0f}});
        ui.setState(handle, UiStateFlags{.visible = true, .enabled = true, .interactable = true});
        return handle;
    }

    void extract(UiSystem& ui)
    {
        ui.extractCommandsRef(800, 600);
    }

    const UiTextData& textPayload(UiSystem& ui, UiHandle handle)
    {
        const UiNode* node = ui.findNode(handle);
        require(node != nullptr, "text node missing");
        return std::get<UiTextData>(node->payload);
    }

    const UiRect& bounds(UiSystem& ui, UiHandle handle)
    {
        const UiNode* node = ui.findNode(handle);
        require(node != nullptr, "node missing");
        return node->computedLayout.bounds;
    }

    void testOneWayFormattingBinding()
    {
        UiSystem ui(nullptr);
        const UiHandle text = createText(ui, ui.getRoot());
        UiObservable<int> gold(25);

        UiBindingDesc desc;
        desc.target = text;
        desc.property = UiBindableProperty::Text;
        desc.source = bindObservable(gold);
        desc.formatter = [](const UiBindingValue& value)
        {
            return "GOLD " + uiBindingValueToString(value);
        };

        const UiBindingId binding = ui.bind(desc);
        require(binding != UI_INVALID_BINDING, "formatted text binding was not created");
        require(textPayload(ui, text).text == "GOLD 25", "initial formatted binding failed");

        gold.set(40);
        const UiBindingFrameResult result = ui.updateBindings();
        require(result.applied == 1, "formatted binding did not apply after source change");
        require(textPayload(ui, text).text == "GOLD 40", "formatted binding did not update text");
    }

    void testComputedProgressBindingWithAnimation()
    {
        UiSystem ui(nullptr);
        gts::ui::UiWidgetContext widgetContext(ui);

        gts::ui::UiProgressBarWidget progress;
        gts::ui::UiProgressBarDesc progressDesc;
        progressDesc.layout = fixedLayout(0.80f, 0.10f);
        progressDesc.value = 0.25f;
        progress.build(widgetContext, ui.getRoot(), progressDesc);

        UiObservable<int> current(25);
        UiObservable<int> maximum(100);
        UiBindingSource source = makeBindingSource(
            [&current, &maximum]()
            {
                const float maxValue = static_cast<float>(std::max(1, maximum.get()));
                return makeUiBindingValue(static_cast<float>(current.get()) / maxValue);
            },
            [&current, &maximum]()
            {
                return (current.revision() << 32u) ^ maximum.revision();
            });

        const UiBindingId binding = progress.bindValue(widgetContext, source, linearTiming());
        require(binding != UI_INVALID_BINDING, "computed progress binding was not created");
        extract(ui);
        require(near(bounds(ui, progress.fill()).width, bounds(ui, progress.root()).width * 0.25f),
                "initial progress binding did not set fill width");

        current.set(75);
        ui.updateBindings();
        require(ui.isAnimating(progress.fill(), UiAnimationProperty::LayoutAnchorMax),
                "progress binding did not start layout animation");
        ui.updateAnimations(0.5f);
        extract(ui);
        require(near(bounds(ui, progress.fill()).width, bounds(ui, progress.root()).width * 0.50f),
                "progress binding animation did not interpolate fill width");

        ui.updateAnimations(0.5f);
        extract(ui);
        require(near(bounds(ui, progress.fill()).width, bounds(ui, progress.root()).width * 0.75f),
                "progress binding animation did not finish at source value");

        progress.destroy(widgetContext);
    }

    void testVisibilityEnableAndLayoutInvalidation()
    {
        UiSystem ui(nullptr);
        const UiHandle text = createText(ui, ui.getRoot());
        const UiHandle rect = createRect(ui, ui.getRoot());
        UiObservable<std::string> label("short");
        UiObservable<bool> visible(true);
        UiObservable<bool> enabled(true);

        UiBindingDesc textBinding;
        textBinding.target = text;
        textBinding.property = UiBindableProperty::Text;
        textBinding.source = bindObservable(label);
        require(ui.bind(textBinding) != UI_INVALID_BINDING, "text binding was not created");

        UiBindingDesc visibleBinding;
        visibleBinding.target = rect;
        visibleBinding.property = UiBindableProperty::Visible;
        visibleBinding.source = bindObservable(visible);
        require(ui.bind(visibleBinding) != UI_INVALID_BINDING, "visibility binding was not created");

        UiBindingDesc enabledBinding;
        enabledBinding.target = rect;
        enabledBinding.property = UiBindableProperty::Enabled;
        enabledBinding.source = bindObservable(enabled);
        require(ui.bind(enabledBinding) != UI_INVALID_BINDING, "enabled binding was not created");

        extract(ui);
        label.set("a much longer label");
        visible.set(false);
        enabled.set(false);
        ui.updateBindings();

        const UiNode* rectNode = ui.findNode(rect);
        require(rectNode != nullptr && !rectNode->state.visible && !rectNode->state.enabled,
                "visibility/enabled bindings did not update retained state");
        require(hasFlag(ui.getDocument().getDirtyFlags(), UiDirtyFlags::Layout),
                "text binding did not invalidate layout");
    }

    void testSourceAndMountCleanup()
    {
        UiSystem ui(nullptr);
        const UiMountId mount = ui.createMount();
        const UiHandle root = ui.mountRoot(mount);
        const UiHandle text = createText(ui, root);
        auto source = std::make_shared<UiObservable<std::string>>("alive");

        UiBindingDesc desc;
        desc.target = text;
        desc.property = UiBindableProperty::Text;
        desc.source = bindObservable(source);
        desc.ownerMount = mount;
        const UiBindingId binding = ui.bind(desc);
        require(binding != UI_INVALID_BINDING, "shared observable binding was not created");
        require(ui.bindingManager().activeCount() == 1, "binding manager did not retain binding");

        source.reset();
        ui.updateBindings();
        require(ui.bindingManager().activeCount() == 0, "destroyed source binding was not pruned");

        source = std::make_shared<UiObservable<std::string>>("mounted");
        desc.source = bindObservable(source);
        const UiBindingId mountBinding = ui.bind(desc);
        require(mountBinding != UI_INVALID_BINDING, "mount binding was not created");
        require(ui.bindingManager().activeCount() == 1, "mount binding was not retained");

        require(ui.destroyMount(mount), "mount destruction failed");
        require(ui.bindingManager().activeCount() == 0, "binding survived mount destruction");
    }

    void testSurfaceLocalBindingCleanup()
    {
        UiSystem ui(nullptr);
        UiSurfaceDesc surfaceDesc;
        surfaceDesc.name = "bindings";
        surfaceDesc.order = 10;
        surfaceDesc.rect = {0.0f, 0.0f, 1.0f, 1.0f};
        const UiSurfaceId surface = ui.createSurface(surfaceDesc);

        const UiHandle defaultText = createText(ui, ui.getRoot());
        const UiHandle surfaceText = ui.createNode(surface, UiNodeType::Text, ui.getRoot(surface));
        ui.setLayout(surface, surfaceText, box(0.0f, 0.0f, 0.4f, 0.1f));
        ui.setPayload(surface, surfaceText, UiTextData{});

        UiObservable<std::string> defaultValue("default");
        UiObservable<std::string> surfaceValue("surface");

        UiBindingDesc defaultBinding;
        defaultBinding.target = defaultText;
        defaultBinding.property = UiBindableProperty::Text;
        defaultBinding.source = bindObservable(defaultValue);
        require(ui.bind(defaultBinding) != UI_INVALID_BINDING, "default binding was not created");

        UiBindingDesc surfaceBinding;
        surfaceBinding.target = surfaceText;
        surfaceBinding.property = UiBindableProperty::Text;
        surfaceBinding.source = bindObservable(surfaceValue);
        require(ui.bind(surface, surfaceBinding) != UI_INVALID_BINDING, "surface binding was not created");

        require(ui.bindingManager().activeCount() == 1, "default binding count incorrect");
        const UiBindingManager* secondaryBindings = ui.bindingManager(surface);
        require(secondaryBindings != nullptr && secondaryBindings->activeCount() == 1,
                "surface binding count incorrect");

        require(ui.destroySurface(surface), "surface destruction failed");
        require(ui.bindingManager().activeCount() == 1, "destroying secondary surface removed default binding");
    }
}

int main()
{
    testOneWayFormattingBinding();
    testComputedProgressBindingWithAnimation();
    testVisibilityEnableAndLayoutInvalidation();
    testSourceAndMountCleanup();
    testSurfaceLocalBindingCleanup();
    return 0;
}
