#pragma once

#include <algorithm>
#include <functional>
#include <optional>
#include <string>
#include <variant>

#include "BitmapFont.h"
#include "UiComposition.h"
#include "UiPanelSkin.h"
#include "UiSystem.h"

namespace gts::ui
{
    struct UiWidgetContext
    {
        explicit UiWidgetContext(UiSystem& inUi)
            : ui(inUi),
              document(inUi.getDocument()),
              surface(inUi.getDefaultSurface()),
              compositionRoot(inUi.getRoot())
        {
        }

        explicit UiWidgetContext(UiCompositionContext& context)
            : ui(context.ui),
              document(context.document),
              resources(context.resources),
              surface(context.surface),
              mount(context.mount),
              compositionRoot(context.root)
        {
        }

        UiSystem& ui;
        UiDocument& document;
        IResourceProvider* resources = nullptr;
        UiSurfaceId surface = UI_DEFAULT_SURFACE;
        UiMountId mount = UI_INVALID_MOUNT;
        UiHandle compositionRoot = UI_INVALID_HANDLE;
    };

    inline UiLayoutSpec fillLayout()
    {
        UiLayoutSpec layout;
        layout.positionMode = UiPositionMode::Anchored;
        layout.widthMode = UiSizeMode::FromAnchors;
        layout.heightMode = UiSizeMode::FromAnchors;
        layout.anchorMin = {0.0f, 0.0f};
        layout.anchorMax = {1.0f, 1.0f};
        return layout;
    }

    inline bool eventTargetsHandle(const UiEvent& event, UiHandle handle)
    {
        if (handle == UI_INVALID_HANDLE)
            return false;
        if (event.target == handle)
            return true;

        return std::find(event.targetPath.begin(), event.targetPath.end(), handle) != event.targetPath.end();
    }

    inline void setWidgetState(UiWidgetContext& context,
                               UiHandle handle,
                               bool visible,
                               bool enabled,
                               bool interactable)
    {
        UiStateFlags state;
        if (const UiNode* node = context.ui.findNode(context.surface, handle))
            state = node->state;

        state.visible = visible;
        state.enabled = enabled;
        state.interactable = interactable;
        if (!visible || !enabled)
        {
            state.hovered = false;
            state.focused = false;
            state.pressed = false;
        }

        context.ui.setState(context.surface, handle, state);
    }

    class UiWidget
    {
    public:
        virtual ~UiWidget() = default;

        UiHandle root() const { return rootHandle; }
        bool valid() const { return rootHandle != UI_INVALID_HANDLE; }

        virtual void destroy(UiWidgetContext& context)
        {
            if (rootHandle != UI_INVALID_HANDLE)
                context.ui.removeNode(context.surface, rootHandle);
            rootHandle = UI_INVALID_HANDLE;
        }

        virtual void onEvent(UiWidgetContext& /*context*/, UiEvent& /*event*/) {}

        bool contains(const UiEvent& event) const
        {
            return eventTargetsHandle(event, rootHandle);
        }

    protected:
        UiHandle rootHandle = UI_INVALID_HANDLE;
    };

    struct UiLabelDesc
    {
        UiLayoutSpec layout;
        std::string text;
        BitmapFont* font = nullptr;
        std::string styleClass = "Text.Body";
        std::optional<UiColor> color;
        std::optional<float> scale;
        UiTextWrapMode wrapMode = UiTextWrapMode::None;
        UiHorizontalAlign horizontalAlign = UiHorizontalAlign::Left;
        UiVerticalAlign verticalAlign = UiVerticalAlign::Top;
        int maxLines = 0;
        bool visible = true;
    };

    class UiLabelWidget : public UiWidget
    {
    public:
        void build(UiWidgetContext& context, UiHandle parent, const UiLabelDesc& desc)
        {
            rootHandle = context.ui.createNode(context.surface, UiNodeType::Text, parent);
            context.ui.setLayout(context.surface, rootHandle, desc.layout);
            setWidgetState(context, rootHandle, desc.visible, false, false);
            applyStyle(context, desc);
            setText(context, desc.text);
            if (desc.font != nullptr)
                context.ui.setTextFont(context.surface, rootHandle, desc.font);
        }

        void setText(UiWidgetContext& context, const std::string& text)
        {
            if (rootHandle == UI_INVALID_HANDLE)
                return;

            UiTextData payload = currentTextPayload(context);
            payload.text = text;
            context.ui.setPayload(context.surface, rootHandle, payload);
        }

        void setVisible(UiWidgetContext& context, bool visible)
        {
            setWidgetState(context, rootHandle, visible, false, false);
        }

    private:
        UiTextData currentTextPayload(UiWidgetContext& context) const
        {
            const UiNode* node = context.ui.findNode(context.surface, rootHandle);
            if (node != nullptr && node->type == UiNodeType::Text)
                return std::get<UiTextData>(node->payload);
            return UiTextData{};
        }

        void applyStyle(UiWidgetContext& context, const UiLabelDesc& desc)
        {
            UiTextData payload;
            payload.text = desc.text;
            payload.wrapMode = desc.wrapMode;
            payload.horizontalAlign = desc.horizontalAlign;
            payload.verticalAlign = desc.verticalAlign;
            payload.maxLines = desc.maxLines;
            if (desc.color)
                payload.color = *desc.color;
            if (desc.scale)
                payload.scale = *desc.scale;
            context.ui.setPayload(context.surface, rootHandle, payload);

            if (!desc.styleClass.empty())
                context.ui.setStyleClass(context.surface, rootHandle, desc.styleClass);
        }
    };

    struct UiPanelDesc
    {
        UiLayoutSpec layout;
        std::string styleClass = "Panel";
        UiThickness padding;
        std::string uniformPaddingMetric;
        bool visible = true;
        bool enabled = true;
        bool interactable = false;
    };

    class UiPanelWidget : public UiWidget
    {
    public:
        void build(UiWidgetContext& context, UiHandle parent, const UiPanelDesc& desc)
        {
            enabled = desc.enabled;
            interactable = desc.interactable;
            rootHandle = context.ui.createNode(context.surface, UiNodeType::Rect, parent);
            context.ui.setLayout(context.surface, rootHandle, desc.layout);
            setWidgetState(context, rootHandle, desc.visible, desc.enabled, desc.interactable);
            if (!desc.styleClass.empty())
                context.ui.setStyleClass(context.surface, rootHandle, desc.styleClass);
            context.ui.setPayload(context.surface, rootHandle, UiRectData{});

            contentHandle = context.ui.createNode(context.surface, UiNodeType::Container, rootHandle);
            UiLayoutSpec contentLayout = fillLayout();
            contentLayout.padding = desc.padding;
            if (!desc.uniformPaddingMetric.empty())
            {
                const float padding = context.ui.theme(context.surface) == nullptr
                    ? 0.0f
                    : context.ui.theme(context.surface)->metricOr(desc.uniformPaddingMetric, 0.0f);
                contentLayout.padding = {padding, padding, padding, padding};
            }
            context.ui.setLayout(context.surface, contentHandle, contentLayout);
            setWidgetState(context, contentHandle, desc.visible, desc.enabled, false);
        }

        UiHandle content() const { return contentHandle; }

        void setVisible(UiWidgetContext& context, bool visible)
        {
            setWidgetState(context, rootHandle, visible, visible && enabled, interactable && visible && enabled);
            setWidgetState(context, contentHandle, visible, visible && enabled, false);
        }

        void destroy(UiWidgetContext& context) override
        {
            UiWidget::destroy(context);
            contentHandle = UI_INVALID_HANDLE;
        }

    private:
        UiHandle contentHandle = UI_INVALID_HANDLE;
        bool enabled = true;
        bool interactable = false;
    };

    struct UiButtonDesc
    {
        UiLayoutSpec layout;
        UiLayoutSpec labelLayout = fillLayout();
        std::string text;
        BitmapFont* font = nullptr;
        std::string styleClass = "Button";
        std::string labelStyleClass = "Text.Body";
        std::optional<UiPanelStateSkin> panelStateSkin;
        std::optional<UiColor> textColor;
        std::optional<float> textScale;
        UiHorizontalAlign horizontalAlign = UiHorizontalAlign::Center;
        UiVerticalAlign verticalAlign = UiVerticalAlign::Middle;
        UiTextWrapMode wrapMode = UiTextWrapMode::None;
        int maxLines = 0;
        bool visible = true;
        bool enabled = true;
        bool consumeClick = true;
        bool preventDefault = true;
        bool focusable = true;
        UiNavigationRole navigationRole = UiNavigationRole::Button;
        std::string navigationGroup;
        int tabIndex = UI_NAVIGATION_AUTO_TAB_INDEX;
        bool wrapNavigation = false;
        std::function<void()> onPressed;
    };

    class UiButtonWidget : public UiWidget
    {
    public:
        void build(UiWidgetContext& context, UiHandle parent, const UiButtonDesc& desc)
        {
            buttonDesc = desc;
            pressed = false;

            rootHandle = context.ui.createNode(context.surface, UiNodeType::Rect, parent);
            context.ui.setLayout(context.surface, rootHandle, desc.layout);
            setWidgetState(context, rootHandle, desc.visible, desc.enabled, desc.enabled);
            if (!desc.styleClass.empty())
                context.ui.setStyleClass(context.surface, rootHandle, desc.styleClass);
            context.ui.setPayload(context.surface, rootHandle, UiRectData{});
            registerNavigation(context);

            labelHandle = context.ui.createNode(context.surface, UiNodeType::Text, rootHandle);
            context.ui.setLayout(context.surface, labelHandle, desc.labelLayout);
            setWidgetState(context, labelHandle, desc.visible, false, false);
            if (!desc.labelStyleClass.empty())
                context.ui.setStyleClass(context.surface, labelHandle, desc.labelStyleClass);
            if (desc.font != nullptr)
                context.ui.setTextFont(context.surface, labelHandle, desc.font);

            applyText(context);
            applyPanelStateSkin(context);
        }

        UiHandle label() const { return labelHandle; }

        void setText(UiWidgetContext& context, const std::string& text)
        {
            buttonDesc.text = text;
            applyText(context);
        }

        void setEnabled(UiWidgetContext& context, bool enabled)
        {
            buttonDesc.enabled = enabled;
            setWidgetState(context, rootHandle, buttonDesc.visible, enabled, enabled);
            setWidgetState(context, labelHandle, buttonDesc.visible, false, false);
            applyPanelStateSkin(context);
        }

        void setVisible(UiWidgetContext& context, bool visible)
        {
            buttonDesc.visible = visible;
            setWidgetState(context, rootHandle, visible, visible && buttonDesc.enabled, visible && buttonDesc.enabled);
            setWidgetState(context, labelHandle, visible, false, false);
            applyPanelStateSkin(context);
        }

        bool consumePressed()
        {
            const bool result = pressed;
            pressed = false;
            return result;
        }

        void onEvent(UiWidgetContext& context, UiEvent& event) override
        {
            if (rootHandle == UI_INVALID_HANDLE || !eventTargetsHandle(event, rootHandle))
                return;

            if (event.type == UiEventType::PointerEnter ||
                event.type == UiEventType::PointerLeave ||
                event.type == UiEventType::PointerDown ||
                event.type == UiEventType::PointerUp)
            {
                applyPanelStateSkin(context);
            }

            if (event.phase != UiEventPhase::Target ||
                (event.type != UiEventType::PointerClick && event.type != UiEventType::NavigationSubmit))
            {
                return;
            }

            const UiNode* node = context.ui.findNode(context.surface, rootHandle);
            if (node == nullptr || !node->state.visible || !node->state.enabled)
                return;

            pressed = true;
            if (buttonDesc.onPressed)
                buttonDesc.onPressed();
            applyPanelStateSkin(context);

            if (buttonDesc.preventDefault)
                event.preventDefault();
            if (buttonDesc.consumeClick)
                event.consume();
        }

        void destroy(UiWidgetContext& context) override
        {
            context.ui.unregisterNavigationNode(context.surface, rootHandle);
            UiWidget::destroy(context);
            labelHandle = UI_INVALID_HANDLE;
            pressed = false;
        }

    private:
        void applyText(UiWidgetContext& context)
        {
            if (labelHandle == UI_INVALID_HANDLE)
                return;

            UiTextData label;
            label.text = buttonDesc.text;
            label.horizontalAlign = buttonDesc.horizontalAlign;
            label.verticalAlign = buttonDesc.verticalAlign;
            label.wrapMode = buttonDesc.wrapMode;
            label.maxLines = buttonDesc.maxLines;
            if (buttonDesc.textColor)
                label.color = *buttonDesc.textColor;
            if (buttonDesc.textScale)
                label.scale = *buttonDesc.textScale;
            context.ui.setPayload(context.surface, labelHandle, label);
        }

        void applyPanelStateSkin(UiWidgetContext& context)
        {
            if (!buttonDesc.panelStateSkin || rootHandle == UI_INVALID_HANDLE)
                return;

            const UiNode* node = context.ui.findNode(context.surface, rootHandle);
            if (node == nullptr)
                return;

            const UiPanelSkin& skin = resolvePanelSkin(*buttonDesc.panelStateSkin, node->state);
            if (skin.type == UiPanelSkinType::SolidColor)
                context.ui.setPayload(context.surface, rootHandle, UiRectData{skin.color});
        }

        void registerNavigation(UiWidgetContext& context)
        {
            if (!buttonDesc.focusable || rootHandle == UI_INVALID_HANDLE)
                return;

            UiNavigationNodeDesc desc;
            desc.role = buttonDesc.navigationRole;
            desc.group = buttonDesc.navigationGroup;
            desc.tabIndex = buttonDesc.tabIndex;
            desc.wrapNavigation = buttonDesc.wrapNavigation;
            desc.activateOnSubmit = true;
            context.ui.registerNavigationNode(context.surface, rootHandle, desc);
        }

        UiButtonDesc buttonDesc;
        UiHandle labelHandle = UI_INVALID_HANDLE;
        bool pressed = false;
    };

    struct UiImageDesc
    {
        UiLayoutSpec layout;
        std::string imageAsset;
        texture_id_type textureID = 0;
        UiColor tint = {1.0f, 1.0f, 1.0f, 1.0f};
        float imageAspect = 1.0f;
        float rotation = 0.0f;
        bool visible = true;
    };

    class UiImageWidget : public UiWidget
    {
    public:
        void build(UiWidgetContext& context, UiHandle parent, const UiImageDesc& desc)
        {
            rootHandle = context.ui.createNode(context.surface, UiNodeType::Image, parent);
            context.ui.setLayout(context.surface, rootHandle, desc.layout);
            setWidgetState(context, rootHandle, desc.visible, false, false);

            UiImageData image;
            image.imageAsset = desc.imageAsset;
            image.textureID = desc.textureID;
            image.tint = desc.tint;
            image.imageAspect = desc.imageAspect;
            image.rotation = desc.rotation;
            context.ui.setPayload(context.surface, rootHandle, image);
        }
    };

    struct UiStackDesc
    {
        UiLayoutSpec layout;
        UiLayoutAxis axis = UiLayoutAxis::Vertical;
        UiLayoutAlignment mainAxisAlignment = UiLayoutAlignment::Start;
        UiLayoutAlignment crossAxisAlignment = UiLayoutAlignment::Stretch;
        float gap = 0.0f;
        std::string gapMetric;
        bool visible = true;
    };

    class UiStackWidget : public UiWidget
    {
    public:
        void build(UiWidgetContext& context, UiHandle parent, const UiStackDesc& desc)
        {
            rootHandle = context.ui.createNode(context.surface, UiNodeType::Container, parent);
            UiLayoutSpec layout = desc.layout;
            layout.layoutMode = UiLayoutMode::Stack;
            layout.stackAxis = desc.axis;
            layout.mainAxisAlignment = desc.mainAxisAlignment;
            layout.crossAxisAlignment = desc.crossAxisAlignment;
            layout.gap = desc.gap;
            if (!desc.gapMetric.empty())
            {
                const UiTheme* theme = context.ui.theme(context.surface);
                if (theme != nullptr)
                    layout.gap = theme->metricOr(desc.gapMetric, desc.gap);
            }
            context.ui.setLayout(context.surface, rootHandle, layout);
            setWidgetState(context, rootHandle, desc.visible, true, false);
        }
    };

    struct UiSpacerDesc
    {
        UiLayoutSpec layout;
        bool visible = true;
    };

    class UiSpacerWidget : public UiWidget
    {
    public:
        void build(UiWidgetContext& context, UiHandle parent, const UiSpacerDesc& desc)
        {
            rootHandle = context.ui.createNode(context.surface, UiNodeType::Container, parent);
            context.ui.setLayout(context.surface, rootHandle, desc.layout);
            setWidgetState(context, rootHandle, desc.visible, true, false);
        }
    };

    struct UiSeparatorDesc
    {
        UiLayoutSpec layout;
        std::string styleClass = "Separator";
        bool visible = true;
    };

    class UiSeparatorWidget : public UiWidget
    {
    public:
        void build(UiWidgetContext& context, UiHandle parent, const UiSeparatorDesc& desc)
        {
            rootHandle = context.ui.createNode(context.surface, UiNodeType::Rect, parent);
            context.ui.setLayout(context.surface, rootHandle, desc.layout);
            setWidgetState(context, rootHandle, desc.visible, false, false);
            if (!desc.styleClass.empty())
                context.ui.setStyleClass(context.surface, rootHandle, desc.styleClass);
            context.ui.setPayload(context.surface, rootHandle, UiRectData{});
        }
    };

    struct UiScrollViewDesc
    {
        UiLayoutSpec layout;
        UiVec2 contentOffset;
        bool visible = true;
    };

    class UiScrollViewWidget : public UiWidget
    {
    public:
        void build(UiWidgetContext& context, UiHandle parent, const UiScrollViewDesc& desc)
        {
            rootHandle = context.ui.createNode(context.surface, UiNodeType::Container, parent);
            UiLayoutSpec layout = desc.layout;
            layout.layoutMode = UiLayoutMode::Scroll;
            layout.clipMode = UiClipMode::ClipChildren;
            layout.contentOffset = desc.contentOffset;
            context.ui.setLayout(context.surface, rootHandle, layout);
            setWidgetState(context, rootHandle, desc.visible, true, false);
        }

        void setContentOffset(UiWidgetContext& context, UiVec2 offset)
        {
            if (rootHandle == UI_INVALID_HANDLE)
                return;

            const UiNode* node = context.ui.findNode(context.surface, rootHandle);
            if (node == nullptr)
                return;

            UiLayoutSpec layout = node->layout;
            layout.contentOffset = offset;
            context.ui.setLayout(context.surface, rootHandle, layout);
        }
    };

    struct UiProgressBarDesc
    {
        UiLayoutSpec layout;
        std::string trackStyleClass = "ProgressBar.Track";
        std::string fillStyleClass = "ProgressBar.Fill";
        float value = 0.0f;
        bool visible = true;
    };

    class UiProgressBarWidget : public UiWidget
    {
    public:
        void build(UiWidgetContext& context, UiHandle parent, const UiProgressBarDesc& desc)
        {
            progressDesc = desc;
            rootHandle = context.ui.createNode(context.surface, UiNodeType::Rect, parent);
            context.ui.setLayout(context.surface, rootHandle, desc.layout);
            setWidgetState(context, rootHandle, desc.visible, false, false);
            if (!desc.trackStyleClass.empty())
                context.ui.setStyleClass(context.surface, rootHandle, desc.trackStyleClass);
            context.ui.setPayload(context.surface, rootHandle, UiRectData{});

            fillHandle = context.ui.createNode(context.surface, UiNodeType::Rect, rootHandle);
            setWidgetState(context, fillHandle, desc.visible, false, false);
            if (!desc.fillStyleClass.empty())
                context.ui.setStyleClass(context.surface, fillHandle, desc.fillStyleClass);
            context.ui.setPayload(context.surface, fillHandle, UiRectData{});
            setValue(context, desc.value);
        }

        UiHandle fill() const { return fillHandle; }

        void setValue(UiWidgetContext& context, float value)
        {
            progressDesc.value = std::clamp(value, 0.0f, 1.0f);

            UiLayoutSpec fill = fillLayout();
            fill.anchorMax = {progressDesc.value, 1.0f};
            context.ui.setLayout(context.surface, fillHandle, fill);
        }

        void destroy(UiWidgetContext& context) override
        {
            UiWidget::destroy(context);
            fillHandle = UI_INVALID_HANDLE;
        }

    private:
        UiProgressBarDesc progressDesc;
        UiHandle fillHandle = UI_INVALID_HANDLE;
    };
} // namespace gts::ui
